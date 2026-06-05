/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/MainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "onSearchTextChanged",
        "",
        "text",
        "onSearchDebounceFired",
        "onTopTabChanged",
        "index",
        "onStoreTemplateChanged",
        "onStoreFeedChanged",
        "onSettingsSectionChanged",
        "row",
        "showDetailsForApp",
        "appId",
        "presentAppDetailsPage",
        "AppInfo",
        "info",
        "installStoreApp",
        "onInstalledUninstallRequested",
        "onCheckForUpdatesTriggered",
        "onUpdateAllTriggered",
        "startNextQueuedUpdate",
        "finishUpdateAll",
        "appHasPendingUpdate",
        "app",
        "refreshUpdateAllButtonState",
        "onClearLeftoverUserDataTriggered",
        "onRefreshRemotesTriggered",
        "onAddRemoteTriggered",
        "onRemoveRemoteTriggered",
        "onTrackedBuildAdd",
        "onTrackedBuildEdit",
        "onTrackedBuildRemove",
        "onTrackedBuildRefresh",
        "onTrackedBuildSelectionChanged",
        "openTrackedBuildDialogForApp",
        "onTrackedBuildInstallRequested",
        "TrackedBuildProject",
        "project",
        "assetUrl",
        "linkedAppId",
        "releaseTag",
        "presentTrackedBuildSetupDialog",
        "TrackedBuildSetupDialog::Mode",
        "mode",
        "existing",
        "TrackedBuildSetupHints",
        "hints"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onSearchTextChanged'
        QtMocHelpers::SlotData<void(const QString &)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'onSearchDebounceFired'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTopTabChanged'
        QtMocHelpers::SlotData<void(int)>(5, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'onStoreTemplateChanged'
        QtMocHelpers::SlotData<void(int)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'onStoreFeedChanged'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'onSettingsSectionChanged'
        QtMocHelpers::SlotData<void(int)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Slot 'showDetailsForApp'
        QtMocHelpers::SlotData<void(const QString &)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'presentAppDetailsPage'
        QtMocHelpers::SlotData<void(const AppInfo &)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 15 },
        }}),
        // Slot 'installStoreApp'
        QtMocHelpers::SlotData<void(const AppInfo &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 15 },
        }}),
        // Slot 'onInstalledUninstallRequested'
        QtMocHelpers::SlotData<void(const QString &)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'onCheckForUpdatesTriggered'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onUpdateAllTriggered'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'startNextQueuedUpdate'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'finishUpdateAll'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'appHasPendingUpdate'
        QtMocHelpers::SlotData<bool(const AppInfo &) const>(22, 2, QMC::AccessPrivate, QMetaType::Bool, {{
            { 0x80000000 | 14, 23 },
        }}),
        // Slot 'refreshUpdateAllButtonState'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearLeftoverUserDataTriggered'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRefreshRemotesTriggered'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAddRemoteTriggered'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRemoveRemoteTriggered'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTrackedBuildAdd'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTrackedBuildEdit'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTrackedBuildRemove'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTrackedBuildRefresh'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTrackedBuildSelectionChanged'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openTrackedBuildDialogForApp'
        QtMocHelpers::SlotData<void(const AppInfo &)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 23 },
        }}),
        // Slot 'onTrackedBuildInstallRequested'
        QtMocHelpers::SlotData<void(const TrackedBuildProject &, const QString &, const QString &, const QString &)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 36, 37 }, { QMetaType::QString, 38 }, { QMetaType::QString, 39 }, { QMetaType::QString, 40 },
        }}),
        // Slot 'presentTrackedBuildSetupDialog'
        QtMocHelpers::SlotData<bool(TrackedBuildSetupDialog::Mode, const TrackedBuildProject &, const TrackedBuildSetupHints &)>(41, 2, QMC::AccessPrivate, QMetaType::Bool, {{
            { 0x80000000 | 42, 43 }, { 0x80000000 | 36, 44 }, { 0x80000000 | 45, 46 },
        }}),
        // Slot 'presentTrackedBuildSetupDialog'
        QtMocHelpers::SlotData<bool(TrackedBuildSetupDialog::Mode, const TrackedBuildProject &)>(41, 2, QMC::AccessPrivate | QMC::MethodCloned, QMetaType::Bool, {{
            { 0x80000000 | 42, 43 }, { 0x80000000 | 36, 44 },
        }}),
        // Slot 'presentTrackedBuildSetupDialog'
        QtMocHelpers::SlotData<bool(TrackedBuildSetupDialog::Mode)>(41, 2, QMC::AccessPrivate | QMC::MethodCloned, QMetaType::Bool, {{
            { 0x80000000 | 42, 43 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onSearchTextChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->onSearchDebounceFired(); break;
        case 2: _t->onTopTabChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->onStoreTemplateChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->onStoreFeedChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->onSettingsSectionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->showDetailsForApp((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->presentAppDetailsPage((*reinterpret_cast<std::add_pointer_t<AppInfo>>(_a[1]))); break;
        case 8: _t->installStoreApp((*reinterpret_cast<std::add_pointer_t<AppInfo>>(_a[1]))); break;
        case 9: _t->onInstalledUninstallRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->onCheckForUpdatesTriggered(); break;
        case 11: _t->onUpdateAllTriggered(); break;
        case 12: _t->startNextQueuedUpdate(); break;
        case 13: _t->finishUpdateAll(); break;
        case 14: { bool _r = _t->appHasPendingUpdate((*reinterpret_cast<std::add_pointer_t<AppInfo>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 15: _t->refreshUpdateAllButtonState(); break;
        case 16: _t->onClearLeftoverUserDataTriggered(); break;
        case 17: _t->onRefreshRemotesTriggered(); break;
        case 18: _t->onAddRemoteTriggered(); break;
        case 19: _t->onRemoveRemoteTriggered(); break;
        case 20: _t->onTrackedBuildAdd(); break;
        case 21: _t->onTrackedBuildEdit(); break;
        case 22: _t->onTrackedBuildRemove(); break;
        case 23: _t->onTrackedBuildRefresh(); break;
        case 24: _t->onTrackedBuildSelectionChanged(); break;
        case 25: _t->openTrackedBuildDialogForApp((*reinterpret_cast<std::add_pointer_t<AppInfo>>(_a[1]))); break;
        case 26: _t->onTrackedBuildInstallRequested((*reinterpret_cast<std::add_pointer_t<TrackedBuildProject>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 27: { bool _r = _t->presentTrackedBuildSetupDialog((*reinterpret_cast<std::add_pointer_t<TrackedBuildSetupDialog::Mode>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<TrackedBuildProject>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<TrackedBuildSetupHints>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 28: { bool _r = _t->presentTrackedBuildSetupDialog((*reinterpret_cast<std::add_pointer_t<TrackedBuildSetupDialog::Mode>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<TrackedBuildProject>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 29: { bool _r = _t->presentTrackedBuildSetupDialog((*reinterpret_cast<std::add_pointer_t<TrackedBuildSetupDialog::Mode>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 30)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 30;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 30)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 30;
    }
    return _id;
}
QT_WARNING_POP
