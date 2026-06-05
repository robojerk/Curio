/****************************************************************************
** Meta object code from reading C++ file 'FlatpakBackend.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/FlatpakBackend.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FlatpakBackend.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
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
struct qt_meta_tag_ZN14FlatpakBackendE_t {};
} // unnamed namespace

template <> constexpr inline auto FlatpakBackend::qt_create_metaobjectdata<qt_meta_tag_ZN14FlatpakBackendE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FlatpakBackend",
        "installedAppsUpdated",
        "",
        "QList<AppInfo>",
        "apps",
        "searchResultsUpdated",
        "storeSuggestionsUpdated",
        "repoId",
        "storeCollectionsUpdated",
        "trending",
        "popular",
        "recent",
        "updated",
        "storeFetchFailed",
        "message",
        "storeCatalogIndexReady",
        "patches",
        "catalogChunkReady",
        "storeCollectionsPatched",
        "storeIconsEnriched",
        "flathubSuggestionsUpdated",
        "flathubCollectionsUpdated",
        "operationStarted",
        "Operation",
        "op",
        "operationProgress",
        "operationFinished",
        "remotesUpdated",
        "QList<std::pair<QString,QString>>",
        "remotes",
        "remoteAddFinished",
        "ok",
        "remoteRemoveFinished",
        "trackedBuildProjectsUpdated",
        "QList<TrackedBuildProject>",
        "projects",
        "trackedBuildRefreshFinished",
        "trackedBuildRefreshFailed",
        "remoteUpdatesChecked",
        "appUpdateCount",
        "runtimeUpdateCount",
        "remoteUpdatesCheckFailed",
        "installedAppsMetadataEnriched",
        "appMetadataEnriched",
        "AppInfo",
        "app",
        "catalogIndexFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'installedAppsUpdated'
        QtMocHelpers::SignalData<void(const QVector<AppInfo> &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'searchResultsUpdated'
        QtMocHelpers::SignalData<void(const QVector<AppInfo> &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'storeSuggestionsUpdated'
        QtMocHelpers::SignalData<void(const QString &, const QVector<AppInfo> &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 3, 4 },
        }}),
        // Signal 'storeCollectionsUpdated'
        QtMocHelpers::SignalData<void(const QString &, const QVector<AppInfo> &, const QVector<AppInfo> &, const QVector<AppInfo> &, const QVector<AppInfo> &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 3, 9 }, { 0x80000000 | 3, 10 }, { 0x80000000 | 3, 11 },
            { 0x80000000 | 3, 12 },
        }}),
        // Signal 'storeFetchFailed'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'storeCatalogIndexReady'
        QtMocHelpers::SignalData<void(const QString &, const QVector<AppInfo> &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 3, 16 },
        }}),
        // Signal 'catalogChunkReady'
        QtMocHelpers::SignalData<void(const QString &, const QVector<AppInfo> &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 3, 4 },
        }}),
        // Signal 'storeCollectionsPatched'
        QtMocHelpers::SignalData<void(const QString &, const QVector<AppInfo> &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 3, 4 },
        }}),
        // Signal 'storeIconsEnriched'
        QtMocHelpers::SignalData<void(const QString &, const QVector<AppInfo> &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 3, 4 },
        }}),
        // Signal 'flathubSuggestionsUpdated'
        QtMocHelpers::SignalData<void(const QVector<AppInfo> &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'flathubCollectionsUpdated'
        QtMocHelpers::SignalData<void(const QVector<AppInfo> &, const QVector<AppInfo> &, const QVector<AppInfo> &, const QVector<AppInfo> &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 9 }, { 0x80000000 | 3, 10 }, { 0x80000000 | 3, 11 }, { 0x80000000 | 3, 12 },
        }}),
        // Signal 'operationStarted'
        QtMocHelpers::SignalData<void(const Operation &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Signal 'operationProgress'
        QtMocHelpers::SignalData<void(const Operation &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Signal 'operationFinished'
        QtMocHelpers::SignalData<void(const Operation &)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Signal 'remotesUpdated'
        QtMocHelpers::SignalData<void(const QVector<QPair<QString,QString>> &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 28, 29 },
        }}),
        // Signal 'remoteAddFinished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 31 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'remoteRemoveFinished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 31 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'trackedBuildProjectsUpdated'
        QtMocHelpers::SignalData<void(const QVector<TrackedBuildProject> &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 34, 35 },
        }}),
        // Signal 'trackedBuildRefreshFinished'
        QtMocHelpers::SignalData<void()>(36, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trackedBuildRefreshFailed'
        QtMocHelpers::SignalData<void(const QString &)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'remoteUpdatesChecked'
        QtMocHelpers::SignalData<void(int, int)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 39 }, { QMetaType::Int, 40 },
        }}),
        // Signal 'remoteUpdatesCheckFailed'
        QtMocHelpers::SignalData<void(const QString &)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'installedAppsMetadataEnriched'
        QtMocHelpers::SignalData<void(const QVector<AppInfo> &)>(42, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 16 },
        }}),
        // Signal 'appMetadataEnriched'
        QtMocHelpers::SignalData<void(const AppInfo &)>(43, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 44, 45 },
        }}),
        // Signal 'catalogIndexFinished'
        QtMocHelpers::SignalData<void(const QString &)>(46, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FlatpakBackend, qt_meta_tag_ZN14FlatpakBackendE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FlatpakBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14FlatpakBackendE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14FlatpakBackendE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14FlatpakBackendE_t>.metaTypes,
    nullptr
} };

void FlatpakBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FlatpakBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->installedAppsUpdated((*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[1]))); break;
        case 1: _t->searchResultsUpdated((*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[1]))); break;
        case 2: _t->storeSuggestionsUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2]))); break;
        case 3: _t->storeCollectionsUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[5]))); break;
        case 4: _t->storeFetchFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->storeCatalogIndexReady((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2]))); break;
        case 6: _t->catalogChunkReady((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2]))); break;
        case 7: _t->storeCollectionsPatched((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2]))); break;
        case 8: _t->storeIconsEnriched((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2]))); break;
        case 9: _t->flathubSuggestionsUpdated((*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[1]))); break;
        case 10: _t->flathubCollectionsUpdated((*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[4]))); break;
        case 11: _t->operationStarted((*reinterpret_cast<std::add_pointer_t<Operation>>(_a[1]))); break;
        case 12: _t->operationProgress((*reinterpret_cast<std::add_pointer_t<Operation>>(_a[1]))); break;
        case 13: _t->operationFinished((*reinterpret_cast<std::add_pointer_t<Operation>>(_a[1]))); break;
        case 14: _t->remotesUpdated((*reinterpret_cast<std::add_pointer_t<QList<std::pair<QString,QString>>>>(_a[1]))); break;
        case 15: _t->remoteAddFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 16: _t->remoteRemoveFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 17: _t->trackedBuildProjectsUpdated((*reinterpret_cast<std::add_pointer_t<QList<TrackedBuildProject>>>(_a[1]))); break;
        case 18: _t->trackedBuildRefreshFinished(); break;
        case 19: _t->trackedBuildRefreshFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->remoteUpdatesChecked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 21: _t->remoteUpdatesCheckFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->installedAppsMetadataEnriched((*reinterpret_cast<std::add_pointer_t<QList<AppInfo>>>(_a[1]))); break;
        case 23: _t->appMetadataEnriched((*reinterpret_cast<std::add_pointer_t<AppInfo>>(_a[1]))); break;
        case 24: _t->catalogIndexFinished((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<AppInfo> & )>(_a, &FlatpakBackend::installedAppsUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<AppInfo> & )>(_a, &FlatpakBackend::searchResultsUpdated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::storeSuggestionsUpdated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QVector<AppInfo> & , const QVector<AppInfo> & , const QVector<AppInfo> & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::storeCollectionsUpdated, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QString & )>(_a, &FlatpakBackend::storeFetchFailed, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::storeCatalogIndexReady, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::catalogChunkReady, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::storeCollectionsPatched, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::storeIconsEnriched, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<AppInfo> & )>(_a, &FlatpakBackend::flathubSuggestionsUpdated, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<AppInfo> & , const QVector<AppInfo> & , const QVector<AppInfo> & , const QVector<AppInfo> & )>(_a, &FlatpakBackend::flathubCollectionsUpdated, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const Operation & )>(_a, &FlatpakBackend::operationStarted, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const Operation & )>(_a, &FlatpakBackend::operationProgress, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const Operation & )>(_a, &FlatpakBackend::operationFinished, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<QPair<QString,QString>> & )>(_a, &FlatpakBackend::remotesUpdated, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(bool , const QString & )>(_a, &FlatpakBackend::remoteAddFinished, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(bool , const QString & )>(_a, &FlatpakBackend::remoteRemoveFinished, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<TrackedBuildProject> & )>(_a, &FlatpakBackend::trackedBuildProjectsUpdated, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)()>(_a, &FlatpakBackend::trackedBuildRefreshFinished, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & )>(_a, &FlatpakBackend::trackedBuildRefreshFailed, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(int , int )>(_a, &FlatpakBackend::remoteUpdatesChecked, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & )>(_a, &FlatpakBackend::remoteUpdatesCheckFailed, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QVector<AppInfo> & )>(_a, &FlatpakBackend::installedAppsMetadataEnriched, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const AppInfo & )>(_a, &FlatpakBackend::appMetadataEnriched, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (FlatpakBackend::*)(const QString & )>(_a, &FlatpakBackend::catalogIndexFinished, 24))
            return;
    }
}

const QMetaObject *FlatpakBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FlatpakBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14FlatpakBackendE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int FlatpakBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 25)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 25;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 25)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 25;
    }
    return _id;
}

// SIGNAL 0
void FlatpakBackend::installedAppsUpdated(const QVector<AppInfo> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void FlatpakBackend::searchResultsUpdated(const QVector<AppInfo> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void FlatpakBackend::storeSuggestionsUpdated(const QString & _t1, const QVector<AppInfo> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void FlatpakBackend::storeCollectionsUpdated(const QString & _t1, const QVector<AppInfo> & _t2, const QVector<AppInfo> & _t3, const QVector<AppInfo> & _t4, const QVector<AppInfo> & _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 4
void FlatpakBackend::storeFetchFailed(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void FlatpakBackend::storeCatalogIndexReady(const QString & _t1, const QVector<AppInfo> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void FlatpakBackend::catalogChunkReady(const QString & _t1, const QVector<AppInfo> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void FlatpakBackend::storeCollectionsPatched(const QString & _t1, const QVector<AppInfo> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 8
void FlatpakBackend::storeIconsEnriched(const QString & _t1, const QVector<AppInfo> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}

// SIGNAL 9
void FlatpakBackend::flathubSuggestionsUpdated(const QVector<AppInfo> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void FlatpakBackend::flathubCollectionsUpdated(const QVector<AppInfo> & _t1, const QVector<AppInfo> & _t2, const QVector<AppInfo> & _t3, const QVector<AppInfo> & _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 11
void FlatpakBackend::operationStarted(const Operation & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1);
}

// SIGNAL 12
void FlatpakBackend::operationProgress(const Operation & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}

// SIGNAL 13
void FlatpakBackend::operationFinished(const Operation & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void FlatpakBackend::remotesUpdated(const QVector<QPair<QString,QString>> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 14, nullptr, _t1);
}

// SIGNAL 15
void FlatpakBackend::remoteAddFinished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1, _t2);
}

// SIGNAL 16
void FlatpakBackend::remoteRemoveFinished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 16, nullptr, _t1, _t2);
}

// SIGNAL 17
void FlatpakBackend::trackedBuildProjectsUpdated(const QVector<TrackedBuildProject> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 17, nullptr, _t1);
}

// SIGNAL 18
void FlatpakBackend::trackedBuildRefreshFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void FlatpakBackend::trackedBuildRefreshFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 19, nullptr, _t1);
}

// SIGNAL 20
void FlatpakBackend::remoteUpdatesChecked(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 20, nullptr, _t1, _t2);
}

// SIGNAL 21
void FlatpakBackend::remoteUpdatesCheckFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 21, nullptr, _t1);
}

// SIGNAL 22
void FlatpakBackend::installedAppsMetadataEnriched(const QVector<AppInfo> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 22, nullptr, _t1);
}

// SIGNAL 23
void FlatpakBackend::appMetadataEnriched(const AppInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 23, nullptr, _t1);
}

// SIGNAL 24
void FlatpakBackend::catalogIndexFinished(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 24, nullptr, _t1);
}
QT_WARNING_POP
