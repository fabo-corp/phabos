#include "connection.h"

void gb_connection_init(struct gb_connection *connection)
{
    list_init(&connection->list);
}
