# for eclipse, minimal Makefile

ifeq ($(GCC),)
	GCC := 0
endif

all:
	scons --no-cache debug=1 gcc=$(GCC)

release:
	scons --no-cache debug=0 gcc=$(GCC)

clean: clean-debug clean-release

clean-debug:
	scons --no-cache -c debug=1 

clean-release:
	scons --no-cache -c debug=0 
	