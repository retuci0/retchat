.PHONY: all linux windows clean

all: linux windows

linux:
	$(MAKE) -C linux -f Makefile.linux

windows:
	@echo "para Windows, usa los scripts en la carpeta windows/"
	@echo "  compile_MINGW.bat  - para MinGW"
	@echo "  compile_MSVC.bat   - para MSVC"

clean:
	$(MAKE) -C linux -f Makefile.linux clean
	rm -rf bin/
