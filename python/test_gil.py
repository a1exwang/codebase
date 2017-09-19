from threading import Thread


s = 0


def thread_fn(n):
    global s
    for i in range(n):
        s += 1


threads = []
for i in range(16):
    thread = Thread(target=thread_fn, args=(100000000000000000000,))
    thread.start()
    threads.append(thread)

for thread in threads:
    thread.join()

print(s)
