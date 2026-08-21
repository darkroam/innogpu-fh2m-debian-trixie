#!/bin/bash

CURRENT_DIR="$( cd -- "$(dirname "$0")" > /dev/null 2>&1 ; pwd -P)"
cd $CURRENT_DIR

BUILD=$1
SOURCES=/lib/modules/`uname -r`/source
ARCH=$2
CC=gcc
ISYSTEM=`gcc -print-file-name=include 2> /dev/null`
TEST_ITEM_FILE="test_item.sh"
CFG_FILE_DIR=$CURRENT_DIR
CFG_FILE="kernel_autocfg.h"

LINK_TEST_DIR="./tools/kernel_api_support_test"
LINK_TEST_FILE="cfg_detect.c"

if [ "$ARCH" = "" ]; then
	ARCH=`uname -m | sed -e 's/aarch64/arm64/' \
		-e 's/loongarch64/loongarch/' \
		-e 's/riscv64/riscv/' \
		-e 's/armv[0-7]\w\+/arm/'`
fi

if [ "$ARCH" = "x86_64" ]; then
	ARCH="x86"
fi

if [[ -n $CROSS_COMPILE ]]; then
	CC=${CROSS_COMPILE}gcc
fi

loongarch64_extern_cflags() {
	local extern_cflags="-include $BUILD/include/linux/compiler_types.h"
	extern_cflags="$extern_cflags -ULOONGARCHEB -U_LOONGARCHEB -U__LOONGARCHEB \
		-U__LOONGARCHEB__ -ULOONGARCHEL -U_LOONGARCHEL -U__LOONGARCHEL \
		-U__LOONGARCHEL__ -DLOONGARCHEL -D_LOONGARCHEL -D__LOONGARCHEL \
		-D__LOONGARCHEL__ -fno-stack-check -U_LOONGARCH_ISA \
		-D_LOONGARCH_ISA=_LOONGARCH_ISA_LOONGARCH64"
	extern_cflags="$extern_cflags -I$BUILD/arch/loongarch/include/asm/mach-la64"
	extern_cflags="$extern_cflags -I$BUILD/arch/loongarch/include/asm/mach-loongson64"
	extern_cflags="$extern_cflags -I$BUILD/arch/loongarch/include/asm/mach-generic"
	echo $extern_cflags
}

aarch64_extern_cflags() {
	local extern_cflags="-include $BUILD/include/linux/compiler_types.h"
	echo $extern_cflags
}

# 有一些架构如果有额外的flag需要添加 在这个函数里面扩展
arch_extern_cflags() {
	REAL_ARCH=`uname -m`
	if [ "$REAL_ARCH" = "loongarch64" ]; then
		loongarch64_extern_cflags
	elif [ "$REAL_ARCH" = "aarch64" ]; then
		aarch64_extern_cflags
	fi
}

prepare_cflags() {
	BASE_CFLAGS="-D__KERNEL__ \
		-DKBUILD_BASENAME=\"cfg_detect\" -DKBUILD_MODNAME=\"cfg_detect\" \
		-nostdinc -isystem $ISYSTEM"

	if [ "$ARCH" = "arm" ]; then
		BASE_CFLAGS="$BASE_CFLAGS -D__LINUX_ARM_ARCH__=7"
	fi

	grep "CONFIG_HAVE_FENTRY \+1" $BUILD/include/generated/autoconf.h > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		BASE_CFLAGS="$BASE_CFLAGS -mfentry -DCC_USING_FENTRY"
	fi

	ARCH_EXTERN_CFLAGS=$(arch_extern_cflags)

	if [ -d $SOURCES ]; then
		KCONFIG_FILE="$SOURCES/include/linux/kconfig.h"
	else
		KCONFIG_FILE="$BUILD/include/linux/kconfig.h"
	fi

	CFLAGS="$BASE_CFLAGS -include $KCONFIG_FILE"
	if [ -n "$CROSS_COMPILE" ] && [ -n "$CROSS_COMPILE_ADD_HEADERS" ]; then
		CFLAGS="$CFLAGS -I$CROSS_COMPILE_ADD_HEADERS"
	fi

	if [ -d $SOURCES ]; then
		SOURCE_HEADERS="$SOURCES/include"
		SOURCE_ARCH_HEADERS="$SOURCES/arch/$ARCH/include"
		CFLAGS="$CFLAGS -I$SOURCE_HEADERS"
		CFLAGS="$CFLAGS -I$SOURCE_HEADERS/uapi"
		CFLAGS="$CFLAGS -I$SOURCE_HEADERS/generated/uapi"
		CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS"
		CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS/uapi"
		CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS/generated"
		CFLAGS="$CFLAGS -I$SOURCE_ARCH_HEADERS/generated/uapi"
	fi

	BUILD_HEADERS="$BUILD/include"
	BUILD_ARCH_HEADERS="$BUILD/arch/$ARCH/include"
	CFLAGS="$CFLAGS -I$BUILD_HEADERS"
	CFLAGS="$CFLAGS -I$BUILD_HEADERS/uapi"
	CFLAGS="$CFLAGS -I$BUILD_HEADERS/generated/uapi"
	CFLAGS="$CFLAGS -I$BUILD_ARCH_HEADERS"
	CFLAGS="$CFLAGS -I$BUILD_ARCH_HEADERS/uapi"
	CFLAGS="$CFLAGS -I$BUILD_ARCH_HEADERS/generated"
	CFLAGS="$CFLAGS -I$BUILD_ARCH_HEADERS/generated/uapi"

	CFLAGS="$CFLAGS $ARCH_EXTERN_CFLAGS"
}

detect_kernel_config_only_compile() {
	echo "$CODE" > cfg_detect.c
	if [ ${DEBUG} = "true" ]; then
		echo -e "${CODE}"
		echo ${DEFINE}
		echo ${TYPE}
		echo ${VAL}
		$CC $CFLAGS -c cfg_detect.c
#		$CC $CFLAGS -Wall -Werror -o cfg_detect --verbose
	else
		$CC $CFLAGS -c cfg_detect.c > /dev/null 2>&1
#		$CC $CFLAGS -Wall -Werror -o cfg_detect > /dev/null 2>&1
	fi
	rm -f cfg_detect.c

	if [ -f cfg_detect.o ]; then
		rm -rf cfg_detect.o
		if [ "${TYPE}" = "success_define" ]; then
			echo "#define ${DEFINE} ${VAL}" >> $CFG_FILE_DIR/$CFG_FILE
		fi
	else
		if [ "${TYPE}" = "fail_define" ]; then
			echo "#define ${DEFINE} ${VAL}" >> $CFG_FILE_DIR/$CFG_FILE
		fi
	fi

	return 0
}

detect_kernel_config_with_link() {
	cd $LINK_TEST_DIR
	echo "$CODE" > $LINK_TEST_FILE
	export INNO_CCFLAGS=$CFLAGS
	if [ ${DEBUG} = "true" ]; then
		echo -e "${CODE}"
		echo ${DEFINE}
		echo ${TYPE}
		echo ${VAL}
		make -j`nproc`
	else
		make -j`nproc` > /dev/null 2>&1
	fi
	rm -f cfg_detect.c

	if [ -f $LINK_TEST_DIR/cfg_detect.ko ]; then
                if [ "${TYPE}" = "success_define" ]; then
                        echo "#define ${DEFINE} ${VAL}" >> $CFG_FILE_DIR/$CFG_FILE
                fi
        else
                if [ "${TYPE}" = "fail_define" ]; then
                        echo "#define ${DEFINE} ${VAL}" >> $CFG_FILE_DIR/$CFG_FILE
                fi
        fi

	make clean > /dev/null 2>&1
	cd -
}

detect_kernel_config() {
	if [ "${NEED_LINK}" = "true" ];then
		detect_kernel_config_with_link
	else
		detect_kernel_config_only_compile
	fi
}

parse_all_test_item() {
	while read line; do
		if [[ $line =~ "TEST_ITEM:{" ]]; then
			CODE=""
			DEFINE=""
			VAL=""
			TYPE=""
			DEBUG=""
			NEED_LINK="false"
			continue
		fi

		if [[ $line =~ "CODE=\"" ]]; then
			code_start="true"
			CODE=`echo $line | awk -F '=' '{ print $NF }' | sed 's/^"//'`
			continue
		fi

		if [[ $line =~ "DEFINE=\"" ]]; then
			code_start="false"
			DEFINE=`echo $line | awk -F '=' '{ print $NF }' | sed 's/"//g'`
			continue
		fi

		if [[ $line =~ "VAL=\"" ]]; then
			code_start="false"
			VAL=`echo $line | awk -F '=' '{ print $NF }' | sed 's/"//g'`
			continue
		fi

		if [[ $line =~ "TYPE=\"" ]]; then
			code_start="false"
			TYPE=`echo $line | awk -F '=' '{ print $NF }' | sed 's/"//g'`
			continue
		fi

		if [[ $line =~ "DEBUG=\"" ]]; then
			code_start="false"
			DEBUG=`echo $line | awk -F '=' '{ print $NF }' | sed 's/"//g'`
			continue
		fi

		if [[ $line =~ "NEED_LINK=\"" ]]; then
			code_start="false"
			NEED_LINK=`echo $line | awk -F '=' '{ print $NF }' | sed 's/"//g'`
			continue
		fi

		if [[ $code_start = "true" ]]; then
			CODE="$CODE\n$line"
			continue
		fi

		if [[ $line = "}" ]]; then
			CODE=`echo -e "${CODE}" | sed 's/"$//'`
			detect_kernel_config
		fi
	done < $TEST_ITEM_FILE
}

clean()
{
	rm -f $CFG_FILE_DIR/$CFG_FILE
}

clean
prepare_cflags
parse_all_test_item
