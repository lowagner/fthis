.PHONY=install

INSTALL_DIRECTORY=/usr/local/bin
# use `.bashrc` if you prefer:
BASH_FILE=$(HOME)/.bash_aliases

install: wthis
	sudo cp $< $(INSTALL_DIRECTORY)
	echo "function fthis() {\n    . $(shell pwd)/fthis.sh \$$@\n}" >> $(BASH_FILE)

wthis: wthis.c
	$(CC) -o $@ $^
