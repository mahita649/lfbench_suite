#ifndef LLIST_H_
#define LLIST_H_

#include "atomic_ops_if.h"
#include <memory>

using namespace std;

typedef intptr_t val_t;

class llist {
    struct node_t {
        val_t data;
        shared_ptr<node_t> next;
    };

    shared_ptr<node_t> head;
    uint32_t size = 0;

    llist(llist&) = delete;
    void operator = (llist&) = delete;

public:
    llist() = default;
    ~llist() = default;

    //class reference {
    //    shared_ptr<node_t> p;
    //    public:
    //        reference(shared_ptr<node_t> p_):p{p_} {}
    //        val_t& operator*() {return p->data;}
    //        val_t* operator->() {return &p->data;}
    //};

    auto list_contains ( val_t val);

    int list_add(val_t val);

    int list_remove();

    int list_size();

};

#endif
