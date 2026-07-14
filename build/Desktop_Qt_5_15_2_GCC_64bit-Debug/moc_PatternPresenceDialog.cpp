/****************************************************************************
** Meta object code from reading C++ file 'PatternPresenceDialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../src/PatternPresenceDialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PatternPresenceDialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_PatternPresenceDialog_t {
    QByteArrayData data[10];
    char stringdata0[151];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_PatternPresenceDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_PatternPresenceDialog_t qt_meta_stringdata_PatternPresenceDialog = {
    {
QT_MOC_LITERAL(0, 0, 21), // "PatternPresenceDialog"
QT_MOC_LITERAL(1, 22, 19), // "finishConfiguration"
QT_MOC_LITERAL(2, 42, 0), // ""
QT_MOC_LITERAL(3, 43, 17), // "showProviderImage"
QT_MOC_LITERAL(4, 61, 5), // "image"
QT_MOC_LITERAL(5, 67, 19), // "handleTestRunButton"
QT_MOC_LITERAL(6, 87, 18), // "handleFinishButton"
QT_MOC_LITERAL(7, 106, 13), // "enterTestMode"
QT_MOC_LITERAL(8, 120, 12), // "exitTestMode"
QT_MOC_LITERAL(9, 133, 17) // "runOnceInTestMode"

    },
    "PatternPresenceDialog\0finishConfiguration\0"
    "\0showProviderImage\0image\0handleTestRunButton\0"
    "handleFinishButton\0enterTestMode\0"
    "exitTestMode\0runOnceInTestMode"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_PatternPresenceDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x08 /* Private */,
       3,    1,   50,    2, 0x08 /* Private */,
       5,    0,   53,    2, 0x08 /* Private */,
       6,    0,   54,    2, 0x08 /* Private */,
       7,    0,   55,    2, 0x08 /* Private */,
       8,    0,   56,    2, 0x08 /* Private */,
       9,    0,   57,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QImage,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void PatternPresenceDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PatternPresenceDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->finishConfiguration(); break;
        case 1: _t->showProviderImage((*reinterpret_cast< const QImage(*)>(_a[1]))); break;
        case 2: _t->handleTestRunButton(); break;
        case 3: _t->handleFinishButton(); break;
        case 4: _t->enterTestMode(); break;
        case 5: _t->exitTestMode(); break;
        case 6: _t->runOnceInTestMode(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject PatternPresenceDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_PatternPresenceDialog.data,
    qt_meta_data_PatternPresenceDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *PatternPresenceDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PatternPresenceDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_PatternPresenceDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int PatternPresenceDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
