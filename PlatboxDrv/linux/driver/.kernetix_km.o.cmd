savedcmd_/home/benedict/platbox_fork/PlatboxDrv/linux/driver/kernetix_km.o := ld -m elf_x86_64 -z noexecstack --no-warn-rwx-segments   -r -o /home/benedict/platbox_fork/PlatboxDrv/linux/driver/kernetix_km.o @/home/benedict/platbox_fork/PlatboxDrv/linux/driver/kernetix_km.mod  ; ./tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --retpoline --rethunk --stackval --static-call --uaccess --prefix=16  --link  --module /home/benedict/platbox_fork/PlatboxDrv/linux/driver/kernetix_km.o

/home/benedict/platbox_fork/PlatboxDrv/linux/driver/kernetix_km.o: $(wildcard ./tools/objtool/objtool)
