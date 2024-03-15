# Shell {#shell}

Turnstone OS a little shell for development and testing purposes. Shell prompt will be shown after pressing enter when boot completes. For supported commands please type **help**.

```
$ help
Commands:
	help		: prints this help
	clear		: clears the screen
	poweroff	: powers off the system alias shutdown
	reboot		: reboots the system
	color		: changes the color first argument foreground second is background in hex
	ps		    : prints the current processes
	date		: prints the current date with time alias time
	usbprobe	: probes the USB bus
	free		: prints the frame usage
	wm	     	: opens test window
	vm	    	: vm commands
	rdtsc		: read timestamp counter
	tosdb		: tosdb commands
	kill		: kills a process with pid
	module		: module(library) utils
```

The most interesting commands are **ps, vm, tosdb, kill** and **module**.

### ps

Shows currently running tasks. Each tasks id may be used for other commands. Task id in hexadecimal and should be used as it.

```
$ ps
	task kernel 0x1 0x43F4DE90 switched 0x1D7FC stack at 0x3FFFFDF8-0x3FFFFE20 heap at 0x40000000[0x4000000] stack 0x40000000[0x10000]
		interruptible 0 sleeping 0 message_waiting 0 interrupt_received 0 future waiting 0 state 3
		message queues 0 messages 0
		heap malloc 0x8F89B free 0x8CABB diff 0x2DE0
	task tosdb_manager 0x2 0x43F43680 switched 0xD932 stack at 0x400000521E78-0x400000521EA0 heap at 0x400001600000[0x10000000] stack 0x400000322000[0x200000]
		interruptible 0 sleeping 0 message_waiting 1 interrupt_received 0 future waiting 0 state 3
		message queues 1 messages 0
		heap malloc 0x1EB36 free 0x1E70E diff 0x428
	task vnet rx 0x3 0x43F427C0 switched 0xA stack at 0x40003DDE3E88-0x40003DDE3EB0 heap at 0x40003D10C000[0x200000] stack 0x40003DDD4000[0x10000]
		interruptible 1 sleeping 0 message_waiting 1 interrupt_received 0 future waiting 0 state 3
		message queues 0 messages 0
		heap malloc 0xA free 0x0 diff 0xA
	task vnet tx 0x4 0x43F421E0 switched 0x26CE stack at 0x40003DDF3F08-0x40003DDF3F30 heap at 0x40003D418000[0x200000] stack 0x40003DDE4000[0x10000]
		interruptible 0 sleeping 0 message_waiting 1 interrupt_received 0 future waiting 0 state 3
		message queues 1 messages 0
		heap malloc 0x2B free 0x19 diff 0x12
	task dhcp 0x5 0x40003D41BE20 switched 0x2 stack at 0x40003E240E38-0x40003E240E60 heap at 0x40003D30C000[0x100000] stack 0x40003E231000[0x10000]
		interruptible 0 sleeping 1 message_waiting 0 interrupt_received 0 future waiting 0 state 3
		message queues 0 messages 0
		heap malloc 0x13 free 0x6 diff 0xD
	task network rx task 0x6 0x43F41AC0 switched 0x9 stack at 0x40003E250E88-0x40003E250EB0 heap at 0x40003D630000[0x200000] stack 0x40003E241000[0x10000]
		interruptible 0 sleeping 0 message_waiting 1 interrupt_received 0 future waiting 0 state 3
		message queues 1 messages 0
		heap malloc 0x50 free 0x3D diff 0x13
	task shell 0x7 0x43F41570 switched 0x48B2A stack at 0x40003D626A68-0x40003D626A90 heap at 0x400037E00000[0x2000000] stack 0x40003D618000[0x10000]
		interruptible 1 sleeping 0 message_waiting 0 interrupt_received 1 future waiting 0 state 2
		message queues 0 messages 0
		heap malloc 0x173E free 0x1721 diff 0x1D
	task vm00000005 0xC 0x43F3D960 switched 0xCC8B stack at 0x40003E260C58-0x40003E260C80 heap at 0x400039E00000[0x200000] stack 0x40003DDF4000[0x4000]
		interruptible 1 sleeping 0 message_waiting 1 interrupt_received 0 future waiting 0 state 3
		message queues 1 messages 0
		heap malloc 0x2D1 free 0x282 diff 0x4F
```

### kill 

Gets one parameter and optionally a **force** parameter. The first paramter is task id (which is in hexadecimal). Preventing page faults sometimes a killed task needs cleanup after killed. **force** parameter does that. Not kills a task. 

### module 

Shows information about loaded kernel modules. It has some pratical but not useful role. Will be changed for supporting VM apps, not kernel.

## tosdb 

It has three paramters: **init, close** and **clear force**. Command is useful during testing. Sometimes **tosdb manager** crashes. And this command is used for restarting it. **clear force** is one argument.

## vm

### vm create entrypoint

Creates a VM which uses given entrypoint as startup address and runs it. Currently one entrypoint is supported **vmtpm** For it s code you can look \ref cc/programs/vm_test_program.64.c. 

### vm id close 

Id is the task id of vm in hexadecimal. Closes VM task and cleanups memory. 

### vm id output 

Displays the output (screen) of VM.

```
$ vm c output
VM 0xC output:
VM Test Program
Hello, World!
This is a test program for the VM
Now halting...
```
