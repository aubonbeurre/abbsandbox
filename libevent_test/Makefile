# for eclipse, minimal Makefile

ifeq ($(GCC),)
	GCC := 0
endif

all:
	python scons.py --no-cache debug=1 gcc=$(GCC)

release:
	python scons.py --no-cache debug=0 gcc=$(GCC)

clean: clean-debug clean-release

clean-debug:
	python scons.py --no-cache -c debug=1 

clean-release:
	python scons.py --no-cache -c debug=0 
