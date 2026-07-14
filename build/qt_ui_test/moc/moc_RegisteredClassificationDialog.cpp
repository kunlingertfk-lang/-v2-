/****************************************************************************
** Meta object code from reading C++ file 'RegisteredClassificationDialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/RegisteredClassificationDialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RegisteredClassificationDialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_RegisteredClassificationDialog_t {
    QByteArrayData data[17];
    char stringdata0[286];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_RegisteredClassificationDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_RegisteredClassificationDialog_t qt_meta_stringdata_RegisteredClassificationDialog = {
    {
QT_MOC_LITERAL(0, 0, 30), // "RegisteredClassificationDialog"
QT_MOC_LITERAL(1, 31, 19), // "finishConfiguration"
QT_MOC_LITERAL(2, 51, 0), // ""
QT_MOC_LITERAL(3, 52, 16), // "runReferenceTest"
QT_MOC_LITERAL(4, 69, 20), // "executeReferenceTest"
QT_MOC_LITERAL(5, 90, 7), // "runTest"
QT_MOC_LITERAL(6, 98, 11), // "importModel"
QT_MOC_LITERAL(7, 110, 11), // "exportModel"
QT_MOC_LITERAL(8, 122, 11), // "deleteModel"
QT_MOC_LITERAL(9, 134, 20), // "openRegisterTraining"
QT_MOC_LITERAL(10, 155, 19), // "openModelManagement"
QT_MOC_LITERAL(11, 175, 20), // "startGlobalDetection"
QT_MOC_LITERAL(12, 196, 24), // "startRectangleRoiEditing"
QT_MOC_LITERAL(13, 221, 16), // "finishRoiEditing"
QT_MOC_LITERAL(14, 238, 16), // "handleRoiChanged"
QT_MOC_LITERAL(15, 255, 3), // "roi"
QT_MOC_LITERAL(16, 259, 26) // "handleRoiSelectionRejected"

    },
    "RegisteredClassificationDialog\0"
    "finishConfiguration\0\0runReferenceTest\0"
    "executeReferenceTest\0runTest\0importModel\0"
    "exportModel\0deleteModel\0openRegisterTraining\0"
    "openModelManagement\0startGlobalDetection\0"
    "startRectangleRoiEditing\0finishRoiEditing\0"
    "handleRoiChanged\0roi\0handleRoiSelectionRejected"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_RegisteredClassificationDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   84,    2, 0x08 /* Private */,
       3,    0,   85,    2, 0x08 /* Private */,
       4,    0,   86,    2, 0x08 /* Private */,
       5,    0,   87,    2, 0x08 /* Private */,
       6,    0,   88,    2, 0x08 /* Private */,
       7,    0,   89,    2, 0x08 /* Private */,
       8,    0,   90,    2, 0x08 /* Private */,
       9,    0,   91,    2, 0x08 /* Private */,
      10,    0,   92,    2, 0x08 /* Private */,
      11,    0,   93,    2, 0x08 /* Private */,
      12,    0,   94,    2, 0x08 /* Private */,
      13,    0,   95,    2, 0x08 /* Private */,
      14,    1,   96,    2, 0x08 /* Private */,
      16,    0,   99,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QRectF,   15,
    QMetaType::Void,

       0        // eod
};

void RegisteredClassificationDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<RegisteredClassificationDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->finishConfiguration(); break;
        case 1: _t->runReferenceTest(); break;
        case 2: _t->executeReferenceTest(); break;
        case 3: _t->runTest(); break;
        case 4: _t->importModel(); break;
        case 5: _t->exportModel(); break;
        case 6: _t->deleteModel(); break;
        case 7: _t->openRegisterTraining(); break;
        case 8: _t->openModelManagement(); break;
        case 9: _t->startGlobalDetection(); break;
        case 10: _t->startRectangleRoiEditing(); break;
        case 11: _t->finishRoiEditing(); break;
        case 12: _t->handleRoiChanged((*reinterpret_cast< const QRectF(*)>(_a[1]))); break;
        case 13: _t->handleRoiSelectionRejected(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject RegisteredClassificationDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_RegisteredClassificationDialog.data,
    qt_meta_data_RegisteredClassificationDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *RegisteredClassificationDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RegisteredClassificationDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_RegisteredClassificationDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int RegisteredClassificationDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 14;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
