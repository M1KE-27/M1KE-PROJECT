<div align="center">

# m1keOS

**A from-scratch operating system — kernel to glass desktop — that puts you in control.**

`x86 · 32-bit` &nbsp;•&nbsp; `preemptive multitasking` &nbsp;•&nbsp; `paging` &nbsp;•&nbsp; `own GUI` &nbsp;•&nbsp; `own style language`

*Naranja y Negro Edition · v0.10*

</div>

---

```
    ___ ___  _ _  ___ ___  ___
   |   |_  || | || __/ _ \/ __|     a real OS, written from zero
   | | | | || | || _| (_) \__ \     in C and x86 assembly
   |_|_|_|_||___||___\___/|___/     no Linux, no fork — its own thing
```

m1keOS is **not** a Linux distro and not a toy "hello world" kernel. It boots from
its own Multiboot kernel into its own glassmorphism desktop, and **every component is
observable, scriptable and replaceable from one control plane** (`m1kectl`). The whole
look is driven by **m1ss** — a CSS-like language you edit and hot-reload at runtime.

> **Philosophy:** friendly by default, 100% technical when you want it. The system
> serves the user. Everything is text-configurable and versionable.

## ✦ Highlights

- 🧠 **Preemptive multitasking** — real kernel tasks, round-robin scheduler, context switch in ASM.
- 🧩 **Memory** — physical bitmap allocator from the Multiboot map + paging (4 MB identity map) + a reentrant heap.
- ⌨️ **Input** — PS/2 keyboard & mouse, Spanish layout with **dead keys** and AltGr.
- 🪟 **Own compositor** — translucent **glass** windows, blur, rounded corners, soft shadows, taskbar, start menu, **dirty-rectangle rendering** (cursor overlay → buttery cursor, zero full-frame redraws when idle).
- 🎨 **m1ss** — our own CSS-like styling language. Edit `/etc/m1ke/theme.m1ss`, run `m1kectl theme reload`, watch the **entire UI** restyle live.
- 🎛️ **m1kectl** — inspect & control kernel, scheduler, memory, modules, services, windows… from the terminal.
- 📞 **Syscalls** — `int 0x80` ABI (write/getpid/sleep/yield/uptime/meminfo/exit).
- 🗂️ **Filesystems** — a VFS mount layer over an in-memory **ramfs**, plus an **ATA/IDE** driver with real, persistent disk read/write.
- 📦 **m1pkg** — a package manager with a built-in repository.

## ✦ The m1ss language

The thing that makes m1keOS *yours*. This is the real file the kernel parses at boot:

```css
/* /etc/m1ke/theme.m1ss — edit, then: m1kectl theme reload */
desktop {
  accent:    #ff8c1a;     /* orange | rose | green | blue | purple | gold | #hex */
  wallpaper: aurora;      /* aurora | gradient | grid | solid */
}
window {
  radius:  14;
  blur:    8;
  opacity: 58;            /* frosted-glass strength 0..100 */
  shadow:  16;
}
taskbar {
  visible:  true;
  position: bottom;       /* bottom | top — yes, you can remove it */
}
```

Change one line, reload, and window borders, blur, the taskbar and terminal colors all
update **live**. The browser demo on the website does the exact same parsing.

## ✦ Quick start

Requirements (Arch/CachyOS): `gcc` (multilib `-m32`), `nasm`, `grub`, `qemu`, `mtools`.

```bash
make all     # build the kernel and m1keos.iso
make gui     # boot the graphical desktop (mouse grabs on hover; Ctrl+Alt+G releases)
make run     # boot headless: drive the shell over the serial terminal
make test    # automated headless smoke test
make clean   # remove build artifacts
```

A `m1kedisk.img` (32 MB) is created automatically and attached as a persistent IDE disk.

## ✦ A taste of the shell

```
m1ke@m1keOS:~$ neofetch                 # show off the system
m1ke@m1keOS:~$ m1kectl inspect mem      # real physical RAM, paging, cr3
m1ke@m1keOS:~$ m1kectl theme set blue   # recolor the whole UI, live
m1ke@m1keOS:~$ m1kectl module unload mouse   # actually masks its IRQ
m1ke@m1keOS:~$ m1kectl disk test        # write+read a real disk sector
m1ke@m1keOS:~$ ps                       # real processes, states, CPU time
m1ke@m1keOS:~$ gui                      # enter the desktop
```

Commands: `ls cd pwd mkdir touch cat write rm tree stat mount · echo clear history ·
mem free uptime uname whoami date ps reboot halt · m1pkg · m1kectl · dmesg · neofetch
cowsay fortune · gui`.

## ✦ Architecture

```
boot/            Multiboot header, SSE enable, entry point
linker.ld        link the kernel at 1 MB
kernel/
  arch/          GDT, IDT, ISR/IRQ, context switch, syscall stub (lowlevel.asm)
  drivers/       serial, console (framebuffer/VGA), pic, timer, keyboard, mouse, ata
  core/          control plane: config (text), events (dmesg), registry, m1kectl
  mm/            pmm (physical), vmm (paging), heap (reentrant)
  process/       preemptive scheduler + tasks
  syscall/       int 0x80 ABI
  fs/            vfs (mount table) + ramfs backend
  gui/           compositor (glass) + m1ss style language
  shell/  pkg/   m1sh shell + m1pkg package manager
web/             code-themed project site (live m1ss playground)
```

## ✦ Roadmap

```
✅ Input · Memory (PMM + paging) · Multitasking + syscalls
✅ VFS + ATA disk (persistent) · Glass rendering + m1ss
⏳ FAT32 · ring3/userspace · richer GUI apps (ANSI terminal, editor, files)
```

## License

Released as open source. See `ABOUT.md` for the author and the story behind the project.

<div align="center">

*Built from zero, with care, by M1KE-27. The system serves you.* 🟠⬛

</div>
