# samplv1_lv2.pro
#
NAME = samplv1

TARGET = $${NAME}_lv2
TEMPLATE = lib
CONFIG += shared plugin

unix {
	LIBS += -L. -l$${NAME}
	PRE_TARGETDEPS += lib$${NAME}.a
}

include(src_lv2.pri)

HEADERS = \
	config.h \
	samplv1_lv2.h \

SOURCES = \
	samplv1_lv2.cpp

unix {

	OBJECTS_DIR = .obj_lv2
	MOC_DIR     = .moc_lv2

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	isEmpty(LIBDIR) {
		TARGET_ARCH = $$system(uname -m)
		contains(TARGET_ARCH, x86_64) {
			LIBDIR = $${PREFIX}/lib64
		} else {
			LIBDIR = $${PREFIX}/lib
		}
	}

	contains(PREFIX, $$system(echo $HOME)) {
		LV2DIR = $${PREFIX}/.lv2
	} else {
		LV2DIR = $${LIBDIR}/lv2
	}

	TARGET_LV2 = $${NAME}.lv2/$${NAME}

	!exists($${TARGET_LV2}.so) {
		system(touch $${TARGET_LV2}.so)
	}

	INSTALLS += target

	target.path  = $${LV2DIR}/$${NAME}.lv2
	target.files = $${TARGET_LV2}.so \
		$${TARGET_LV2}.ttl \
		$${NAME}.lv2/manifest.ttl

	CONFIG(release, debug|release):QMAKE_POST_LINK += strip $(TARGET);
	QMAKE_POST_LINK += $${QMAKE_COPY} -vp $(TARGET) $${TARGET_LV2}.so

	QMAKE_CLEAN += $${TARGET_LV2}.so
}

QT -= gui
QT += xml

