#include <asm/byteordering.h>
#include <asm/irq.h>

#include <phabos/greybus.h>
#include <phabos/greybus/ap.h>
#include <phabos/serial/uart.h>

#include "../uart-gb.h"

#include <errno.h>

extern struct driver gb_uart_driver;
static struct gb_device *gb_device;
static struct semaphore rx_semaphore;
static struct list_head rx_fifo;

struct uart_fifo_entry {
    struct gb_operation *operation;
    unsigned offset;
    struct list_head list;
};

static ssize_t gb_uart_write(struct file *file, const void *buf, size_t count)
{
    struct gb_operation *op;
    struct gb_uart_send_data_request *req;
    int retval = 0;

    op = gb_operation_create(gb_device->bus, gb_device->cport,
                             GB_UART_PROTOCOL_SEND_DATA, sizeof(*req) + count);
    if (!op)
        return -ENOMEM;

    req = gb_operation_get_request_payload(op);
    req->size = cpu_to_le16(count);
    memcpy(req->data, buf, count);

    retval = gb_operation_send_request_sync(op);
    if (retval)
        goto out;

    if (gb_operation_get_request_result(op) != GB_OP_SUCCESS)
        retval = -EIO;
    else
        retval = count;

out:
    gb_operation_destroy(op);

    return retval;
}

static ssize_t gb_uart_read(struct file *file, void *buf, size_t count)
{
    struct uart_fifo_entry *uart_buffer;
    struct gb_uart_receive_data_request *req;
    ssize_t nread;

    semaphore_down(&rx_semaphore);

    irq_disable();
    uart_buffer = list_first_entry(&rx_fifo, struct uart_fifo_entry, list);
    irq_enable();

    req = gb_operation_get_request_payload(uart_buffer->operation);

    nread = MIN(count, le16_to_cpu(req->size) - uart_buffer->offset);
    memcpy(buf, (char*) req->data + uart_buffer->offset, nread);
    uart_buffer->offset += nread;

    if (uart_buffer->offset >= le16_to_cpu(req->size)) {
        irq_disable();
        list_del(&uart_buffer->list);
        irq_enable();

        gb_operation_unref(uart_buffer->operation);
        kfree(uart_buffer);
    }

    return nread;
}

static uint8_t gb_uart_rx_data(struct gb_operation *op)
{
    struct uart_fifo_entry *uart_buffer;
    struct gb_uart_receive_data_request *req;
    size_t request_size = gb_operation_get_request_payload_size(op);

    req = gb_operation_get_request_payload(op);

    if (request_size < sizeof(*req))
        return GB_OP_INVALID;

    if (request_size < sizeof(*req) + le16_to_cpu(req->size))
        return GB_OP_INVALID;

    uart_buffer = kzalloc(sizeof(*uart_buffer), MM_KERNEL);
    if (!uart_buffer)
        return -ENOMEM;

    list_init(&uart_buffer->list);
    uart_buffer->operation = op;

    gb_operation_ref(op);

    irq_disable();
    list_add(&rx_fifo, &uart_buffer->list);
    irq_enable();

    semaphore_up(&rx_semaphore);

    return GB_OP_SUCCESS;
}

static struct gb_operation_handler gb_uart_handlers[] = {
    GB_HANDLER(GB_UART_PROTOCOL_RECEIVE_DATA, gb_uart_rx_data),
};

static struct gb_driver uart_driver = {
    .op_handlers = (struct gb_operation_handler*) gb_uart_handlers,
    .op_handlers_count = ARRAY_SIZE(gb_uart_handlers),
};

static int gb_uart_init_device(struct device *device)
{
    device->name = "gb-ap-uart";
    device->description = "Greybus AP UART PHY Protocol";
    device->driver = "gb-ap-uart-phy";

    return 0;
}

static struct gb_protocol uart_protocol = {
    .id = GB_PROTOCOL_UART,
    .init_device = gb_uart_init_device,
};

static struct file_operations gb_uart_ops = {
    .read = gb_uart_read,
    .write = gb_uart_write,
};

static int gb_uart_probe(struct device *device)
{
    int retval;
    struct uart_device *uart;
    dev_t devnum;

    gb_device = containerof(device, struct gb_device, device);

    retval = gb_register_driver(gb_device->bus, gb_device->cport, &uart_driver);
    if (retval)
        return retval;

    gb_listen(gb_device->bus, gb_device->cport);

    uart = kzalloc(sizeof(*uart), MM_KERNEL);

    uart->tty.device.ops = gb_uart_ops;
    uart->tty.device.driver = gb_uart_driver.name;

    retval = devnum_alloc(&gb_uart_driver, &uart->tty.device, &devnum);
    if (retval)
        goto error_devnum_alloc;

    uart->rx_start = uart->rx_end = 0;
    uart->tx_start = uart->tx_end = 0;
    mutex_init(&uart->rx_mutex);
    mutex_init(&uart->tx_mutex);
    semaphore_init(&uart->rx_semaphore, 0);
    semaphore_init(&uart->tx_semaphore, 0);

    semaphore_init(&rx_semaphore, 0);
    list_init(&rx_fifo);

    return tty_register(&uart->tty, devnum);

error_devnum_alloc:
    kfree(uart);
    return retval;
}

static int gb_uart_init(struct driver *driver)
{
    return gb_protocol_register(&uart_protocol);
}

__driver__ struct driver gb_uart_driver = {
    .name = "gb-ap-uart-phy",
    .init = gb_uart_init,
    .probe = gb_uart_probe,
};
