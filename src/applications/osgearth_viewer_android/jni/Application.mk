#ANDROID APPLICATION MAKEFILE
APP_BUILD_SCRIPT := $(call my-dir)/Android.mk
#APP_PROJECT_PATH := $(call my-dir)

APP_OPTIM := debug

APP_PLATFORM 	:= 14
#APP_STL 		:= gnustl_static
APP_CPPFLAGS 	:= -fexceptions -frtti
APP_ABI 		:= armeabi-v7a
APP_MODULES     := osgNativeLib
#NDK_TOOLCHAIN_VERSION=4.7
