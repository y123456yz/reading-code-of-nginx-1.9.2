
default:	build

clean:
	rm -rf Makefile objs

build:
	$(MAKE) -f objs/Makefile
	$(MAKE) -f objs/Makefile manpage

install:
	$(MAKE) -f objs/Makefile install

upgrade:
	/mnt/data/nginx/sbin/nginx -t

	kill -USR2 `cat /mnt/data/nginx/logs/nginx.pid`
	sleep 1
	test -f /mnt/data/nginx/logs/nginx.pid.oldbin

	kill -QUIT `cat /mnt/data/nginx/logs/nginx.pid.oldbin`
