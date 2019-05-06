#include <common.h>
#include <thread.h>
#include <syscall.h>
#include <spinlock.h>
#include <semaphore.h>
#include <debug.h>

void semaphore_init(struct semaphore *sem, const char *name, int value) {
  spinlock_init(&sem->lock, "Sem Lock");
  sem->name = name;
  sem->value = value;
}

void semaphore_wait(struct semaphore *sem) {
  spinlock_acquire(&sem->lock);
  while (sem->value <= 0) {
    asm volatile ("int $0x80" : : "a"(SYS_sem_wait), "b"(sem), "c"(&sem->lock));
  }
  __sync_synchronize();
  --sem->value;
  spinlock_release(&sem->lock);
}

void semaphore_signal(struct semaphore *sem) {
  spinlock_acquire(&sem->lock);
  ++sem->value;
  __sync_synchronize();
  asm volatile ("int $0x80" : : "a"(SYS_sem_signal), "b"(sem));
  spinlock_release(&sem->lock);
}
