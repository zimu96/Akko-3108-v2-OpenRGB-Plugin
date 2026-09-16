/****************************************************************************
** Meta object code from reading C++ file 'AkkoKeyboardPlugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/AkkoKeyboardPlugin.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AkkoKeyboardPlugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN18AkkoKeyboardPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto AkkoKeyboardPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN18AkkoKeyboardPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AkkoKeyboardPlugin"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AkkoKeyboardPlugin, qt_meta_tag_ZN18AkkoKeyboardPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AkkoKeyboardPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18AkkoKeyboardPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18AkkoKeyboardPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18AkkoKeyboardPluginE_t>.metaTypes,
    nullptr
} };

void AkkoKeyboardPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AkkoKeyboardPlugin *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *AkkoKeyboardPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AkkoKeyboardPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18AkkoKeyboardPluginE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "OpenRGBPluginInterface"))
        return static_cast< OpenRGBPluginInterface*>(this);
    if (!strcmp(_clname, "org.openrgb.OpenRGBPluginInterface"))
        return static_cast< OpenRGBPluginInterface*>(this);
    return QObject::qt_metacast(_clname);
}

int AkkoKeyboardPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_AkkoKeyboardPlugin[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x22,  'o',  'r',  'g',  '.',  'o', 
    'p',  'e',  'n',  'r',  'g',  'b',  '.',  'O', 
    'p',  'e',  'n',  'R',  'G',  'B',  'P',  'l', 
    'u',  'g',  'i',  'n',  'I',  'n',  't',  'e', 
    'r',  'f',  'a',  'c',  'e', 
    // "className"
    0x03,  0x72,  'A',  'k',  'k',  'o',  'K',  'e', 
    'y',  'b',  'o',  'a',  'r',  'd',  'P',  'l', 
    'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xa7,  0x66,  'C',  'o',  'm',  'm',  'i', 
    't',  0x6c,  'a',  'k',  'k',  'o',  '-',  '3', 
    '1',  '0',  '8',  '-',  'v',  '2',  0x63,  'I', 
    'I',  'D',  0x78,  0x22,  'o',  'r',  'g',  '.', 
    'o',  'p',  'e',  'n',  'r',  'g',  'b',  '.', 
    'O',  'p',  'e',  'n',  'R',  'G',  'B',  'P', 
    'l',  'u',  'g',  'i',  'n',  'I',  'n',  't', 
    'e',  'r',  'f',  'a',  'c',  'e',  0x62,  'I', 
    'd',  0x78,  0x1a,  'O',  'p',  'e',  'n',  'R', 
    'G',  'B',  '-',  'A',  'k',  'k',  'o',  'K', 
    'e',  'y',  'b',  'o',  'a',  'r',  'd',  'P', 
    'l',  'u',  'g',  'i',  'n',  0x64,  'N',  'a', 
    'm',  'e',  0x74,  'A',  'k',  'k',  'o',  ' ', 
    'K',  'e',  'y',  'b',  'o',  'a',  'r',  'd', 
    ' ',  'E',  'd',  'i',  't',  'o',  'r',  0x77, 
    'O',  'p',  'e',  'n',  'R',  'G',  'B',  'P', 
    'l',  'u',  'g',  'i',  'n',  'A',  'P',  'I', 
    'V',  'e',  'r',  's',  'i',  'o',  'n',  0x05, 
    0x63,  'U',  'r',  'l',  0x78,  0x32,  'h',  't', 
    't',  'p',  's',  ':',  '/',  '/',  'g',  'i', 
    't',  'h',  'u',  'b',  '.',  'c',  'o',  'm', 
    '/',  'O',  'p',  'e',  'n',  'R',  'G',  'B', 
    '/',  'O',  'p',  'e',  'n',  'R',  'G',  'B', 
    '/',  'w',  'i',  'k',  'i',  '/',  'P',  'l', 
    'u',  'g',  'i',  'n',  '-',  'S',  'D',  'K', 
    0x6a,  'V',  'e',  'r',  's',  'i',  'o',  'n', 
    'S',  't',  'r',  0x65,  '1',  '.',  '0',  '.', 
    '0', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(AkkoKeyboardPlugin, AkkoKeyboardPlugin, qt_pluginMetaDataV2_AkkoKeyboardPlugin)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_AkkoKeyboardPlugin[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x22,  'o',  'r',  'g',  '.',  'o', 
    'p',  'e',  'n',  'r',  'g',  'b',  '.',  'O', 
    'p',  'e',  'n',  'R',  'G',  'B',  'P',  'l', 
    'u',  'g',  'i',  'n',  'I',  'n',  't',  'e', 
    'r',  'f',  'a',  'c',  'e', 
    // "className"
    0x03,  0x72,  'A',  'k',  'k',  'o',  'K',  'e', 
    'y',  'b',  'o',  'a',  'r',  'd',  'P',  'l', 
    'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xa7,  0x66,  'C',  'o',  'm',  'm',  'i', 
    't',  0x6c,  'a',  'k',  'k',  'o',  '-',  '3', 
    '1',  '0',  '8',  '-',  'v',  '2',  0x63,  'I', 
    'I',  'D',  0x78,  0x22,  'o',  'r',  'g',  '.', 
    'o',  'p',  'e',  'n',  'r',  'g',  'b',  '.', 
    'O',  'p',  'e',  'n',  'R',  'G',  'B',  'P', 
    'l',  'u',  'g',  'i',  'n',  'I',  'n',  't', 
    'e',  'r',  'f',  'a',  'c',  'e',  0x62,  'I', 
    'd',  0x78,  0x1a,  'O',  'p',  'e',  'n',  'R', 
    'G',  'B',  '-',  'A',  'k',  'k',  'o',  'K', 
    'e',  'y',  'b',  'o',  'a',  'r',  'd',  'P', 
    'l',  'u',  'g',  'i',  'n',  0x64,  'N',  'a', 
    'm',  'e',  0x74,  'A',  'k',  'k',  'o',  ' ', 
    'K',  'e',  'y',  'b',  'o',  'a',  'r',  'd', 
    ' ',  'E',  'd',  'i',  't',  'o',  'r',  0x77, 
    'O',  'p',  'e',  'n',  'R',  'G',  'B',  'P', 
    'l',  'u',  'g',  'i',  'n',  'A',  'P',  'I', 
    'V',  'e',  'r',  's',  'i',  'o',  'n',  0x05, 
    0x63,  'U',  'r',  'l',  0x78,  0x32,  'h',  't', 
    't',  'p',  's',  ':',  '/',  '/',  'g',  'i', 
    't',  'h',  'u',  'b',  '.',  'c',  'o',  'm', 
    '/',  'O',  'p',  'e',  'n',  'R',  'G',  'B', 
    '/',  'O',  'p',  'e',  'n',  'R',  'G',  'B', 
    '/',  'w',  'i',  'k',  'i',  '/',  'P',  'l', 
    'u',  'g',  'i',  'n',  '-',  'S',  'D',  'K', 
    0x6a,  'V',  'e',  'r',  's',  'i',  'o',  'n', 
    'S',  't',  'r',  0x65,  '1',  '.',  '0',  '.', 
    '0', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(AkkoKeyboardPlugin, AkkoKeyboardPlugin)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
