/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Copyright (C) 2018 Institute of Computing
 * Technology, CAS Author : Han Shukai (email :
 * hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Changelog: 2019-8 Reimplement queue.h.
 * Provide Linux-style doube-linked list instead of original
 * unextendable Queue implementation. Luming
 * Wang(wangluming@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * */

#ifndef INCLUDE_LIST_H_
#define INCLUDE_LIST_H_

#include <type.h>
#include <printk.h>

// double-linked list
typedef struct list_node
{
    //pcb_t * ptr_to_pcb;
    struct list_node *next, *prev;
} list_node_t;

typedef list_node_t list_head;

// LIST_HEAD is used to define the head of a list.
#define LIST_HEAD(name) struct list_node name = {&(name), &(name)}

/* TODO: [p2-task1] implement your own list API */
static inline list_node_t * Dequeue(list_head * l)
{
    list_node_t * tail = l->prev;
    list_node_t * head = l->next;
    if((tail == l) && (head == l))
    {
        //如果队列为空
        return NULL;
    }
    else if(head == tail){
        //只有1个元素
        l->prev = l->next = l;
        return head;
    }
    else{
        //队列原本有元素
        //队头元素出队
        list_node_t * pre = head->prev;
        pre->next = l;
        l->next = pre;
        return head;
    }
}

static inline void Enqueue(list_head * l, list_node_t * new_node)
{
    list_node_t * tail = l->prev;
    list_node_t * head = l->next;
    if((tail == l) && (head == l))
    {
        //如果队列原本为空
        l->prev = l->next = new_node;
        new_node->prev = new_node->next = l;
    }
    else{
        //队列原本有元素
        //新元素入队尾
        l->prev = new_node;
        new_node->next = tail;
        new_node->prev = l;
        tail->prev = new_node;
    }
}

static inline void Print_queue(list_head * l)
{
    //printl("now the queue is: \n");
    list_node_t * tail = l->prev;
    list_node_t * head = l->next;
    list_node_t * visit = tail;

    if((tail == l) && (head == l))
    {
        printl("empty queue!\n\n");
        return;
    }

    printl("tail: ");
    do{
        printl("%x --> ", visit);
        visit = visit->next;
    }while(visit!=l);
    printl("head\n\n");
}

static inline int if_queue_empty(list_head * l)
{
    return((l->prev == l) && (l->next == l));
}

static inline void init_queue(list_head * l)
{
    l->prev = l;
    l->next = l;
}

#endif
