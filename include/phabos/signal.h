#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <phabos/list.h>

struct signal {
    unsigned id;
    struct list_head list;
};

#endif /* __SIGNAL_H__ */

