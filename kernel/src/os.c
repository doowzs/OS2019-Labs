#include <common.h>
#include <os.h>
#include <devices.h>
#include <syscall.h>
#include <threads.h>
#include <spinlock.h>
#include <semaphore.h>

struct spinlock printf_lock __attribute__((used));
struct spinlock os_trap_lock;

static struct os_handler root_handler = {
  0, _EVENT_NULL, NULL, NULL
};


sem_t sem_p;
sem_t sem_c;
sem_t mutex;
void customer(void *arg) {
  device_t *tty = dev_lookup("tty1");
  while (1) {
    kmt->sem_wait(&sem_c);
    kmt->sem_wait(&mutex);
    tty->ops->write(tty, 0, ")", 1);
    CLog(BG_RED, ")");
    kmt->sem_signal(&mutex);
    kmt->sem_signal(&sem_p);
  }
}
void producer(void *arg) {
  device_t *tty = dev_lookup("tty1");
  while (1) {
    kmt->sem_wait(&sem_p);
    kmt->sem_wait(&mutex);
    tty->ops->write(tty, 0, "(", 1);
    CLog(BG_RED, "(");
    kmt->sem_signal(&mutex);
    kmt->sem_signal(&sem_c);
  }
}
struct spinlock sprintf_lock;
void echo_task(void *name) {
  device_t *tty = dev_lookup(name);
  char text[128], line[128];
  while (1) {
    int nread = tty->ops->read(tty, 0, line, 128);
    line[nread - 1] = '\0';
    sprintf(text, "Echo: %s.", line);
    tty->ops->write(tty, 0, text, strlen(text));
  }
}

static void os_init() {
  spinlock_init(&printf_lock, "Printf SPIN LOCK");
  spinlock_init(&os_trap_lock, "OS TRAP SPIN LOCK");
  CLog(BG_GREEN, "locks ok");

  pmm->init();
  CLog(BG_GREEN, "pmm ok");

  kmt->init();
  CLog(BG_GREEN, "kmt ok");

  //_vme_init(pmm->alloc, pmm->free);
  //CLog(BG_GREEN, "vme ok");
  //CLog(BG_RED, "vme not enabled!!!!!!");
  
  dev->init();
  CLog(BG_GREEN, "dev ok");

  //create proc here
  kmt->sem_init(&sem_p, "Producer SEM", 5);
  kmt->sem_init(&sem_c, "Customer SEM", 0);
  kmt->sem_init(&mutex, "Producer-Customer MUTEX", 1);
  for (int i = 0; i < 0; ++i) {
    kmt->create(pmm->alloc(sizeof(task_t)), "Producer Task", producer, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "Customer Task", customer, NULL);
  }

  kmt->spin_init(&sprintf_lock, "sprintf-lock");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo-1", echo_task, "tty1");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo-2", echo_task, "tty2");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo-3", echo_task, "tty3");
  kmt->create(pmm->alloc(sizeof(task_t)), "echo-4", echo_task, "tty4");
  /*
  */
}

static void os_run() {
  _intr_write(1);
  while (1) {
    _yield();
  }
}

static _Context *os_trap(_Event ev, _Context *context) {
  if (ev.event == _EVENT_IRQ_TIMER) return context;
  CLog(BG_CYAN, "Event %d: %s", ev.event, ev.msg);

  // important: if this is to sleep, we must
  // release the lock now before acquiring next lock
  if (ev.event == _EVENT_SYSCALL && context->GPR1 == SYS_sleep) {
    spinlock_release((struct spinlock *) context->ecx);
  }
  Assert(ev.event != _EVENT_YIELD || cpu_no_spinlock(), "not allowed to yield with lock acquired");

  bool holding = spinlock_holding(&os_trap_lock);
  if (holding) {
    switch (ev.event) {
      case _EVENT_IRQ_TIMER:
        Panic("No timer interrupt during trap.");
      case _EVENT_IRQ_IODEV:
        break;
      case _EVENT_YIELD:
        Panic("No yield inside trap.");
      case _EVENT_SYSCALL:
        switch(context->eax) {
          case SYS_sleep:
            Panic("No semaphore wait/sleep inside trap.");
        }
    }
  } else {
    spinlock_acquire(&os_trap_lock);
    CLog(FG_PURPLE, ">>>>>> IN TO TRAP");
  }

  _Context *ret = NULL;
  for (struct os_handler *hp = root_handler.next; hp != NULL; hp = hp->next) {
    if (
      (!holding && hp->event == _EVENT_NULL) 
      || hp->event == ev.event
    ) {
      CLog(FG_CYAN, "Handler seq %d", hp->seq);
      _Context *next = hp->handler(ev, context);
      if (next) ret = next;
    }
  }

  struct spinlock *lock = wakeup_reacquire_lock;
  wakeup_reacquire_lock = NULL;
  if (!holding) {
    CLog(FG_PURPLE, "<<<<<< OUT OF TRAP");
    spinlock_release(&os_trap_lock);
  }
  if (lock) {
    spinlock_acquire(lock);
    //printf("[%d] lock %s reacquired\n", _cpu(), lock->name);
  }

  Assert(holding || ret, "Returning to a null context after normal trap.");
  return holding ? context : ret;
}

static void os_on_irq(int seq, int event, handler_t handler) {
  CLog(BG_PURPLE, "Handler for event class %d (seq=%d) added.", event, seq);
  struct os_handler *oh = pmm->alloc(sizeof(struct os_handler));
  oh->seq = seq;
  oh->event = event;
  oh->handler = handler;
  oh->next = NULL;

  spinlock_acquire(&os_trap_lock);
  struct os_handler *hp = &root_handler;
  while (hp->next && hp->next->seq < seq) hp = hp->next;
  oh->next = hp->next;
  hp->next = oh;
  spinlock_release(&os_trap_lock);
}

MODULE_DEF(os) {
  .init   = os_init,
  .run    = os_run,
  .trap   = os_trap,
  .on_irq = os_on_irq,
};
