#include "SchemeStore.h"
#include "MainWindow.h"
#include "ToolsDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include <iostream>

namespace {

QString g_lastMessageText;

int fail(const QString &message)
{
    std::cerr << message.toStdString() << std::endl;
    return 1;
}

ToolConfig makeTool(const QString &id,
                    const QString &name,
                    ToolType type,
                    ToolCategory category)
{
    ToolConfig config;
    config.toolId = id;
    config.toolName = name;
    config.displayName = name;
    config.toolType = type;
    config.category = category;
    config.enabled = true;
    config.params.insert(QStringLiteral("fixture"), name);
    return config;
}

ToolConfig makeDependentTool(const QString &id, const QString &producerId)
{
    ToolConfig config = makeTool(id,
                                 QStringLiteral("依赖工具"),
                                 ToolType::PositionCorrection,
                                 ToolCategory::Location);
    QJsonObject runPoseSource;
    runPoseSource.insert(QStringLiteral("producerId"), producerId);
    QJsonObject correction;
    correction.insert(QStringLiteral("runPoseSource"), runPoseSource);
    correction.insert(QStringLiteral("referencePoseSourceId"), producerId);
    config.params.insert(QStringLiteral("positionCorrection"), correction);
    return config;
}

ToolPreviewSnapshot makeSnapshot(const ToolConfig &config)
{
    ToolPreviewSnapshot snapshot;
    snapshot.valid = true;
    snapshot.toolId = config.toolId;
    snapshot.toolType = config.toolType;
    snapshot.sourceType = QStringLiteral("referenceImage");
    snapshot.result.toolId = config.toolId;
    snapshot.result.toolType = config.toolType;
    snapshot.result.success = true;
    snapshot.result.ok = true;
    snapshot.statusText = QStringLiteral("fixture snapshot");
    snapshot.ok = true;
    return snapshot;
}

QMessageBox *activeMessageBox()
{
    if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        return box;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(widget))
            return box;
    }
    return nullptr;
}

void answerNextMessage(QMessageBox::StandardButton answer)
{
    g_lastMessageText.clear();
    QTimer::singleShot(0, [answer]() {
        QMessageBox *box = activeMessageBox();
        if (!box)
            return;
        g_lastMessageText = box->text();
        if (QAbstractButton *button = box->button(answer))
            button->click();
        else
            box->accept();
    });
}

QToolButton *button(ToolsDialog &dialog, const char *name)
{
    return dialog.findChild<QToolButton *>(QString::fromLatin1(name));
}

bool selectTool(ToolsDialog &dialog, int index)
{
    const QList<QFrame *> cards = dialog.findChildren<QFrame *>(QStringLiteral("toolItemCard"));
    for (QFrame *card : cards) {
        if (card->property("toolIndex").toInt() != index)
            continue;
        QMouseEvent release(QEvent::MouseButtonRelease,
                            card->rect().center(),
                            Qt::LeftButton,
                            Qt::LeftButton,
                            Qt::NoModifier);
        QApplication::sendEvent(card, &release);
        return true;
    }
    return false;
}

QString selectedToolId(const ToolsDialog &dialog)
{
    const QList<QFrame *> cards = dialog.findChildren<QFrame *>(QStringLiteral("toolItemCard"));
    for (QFrame *card : cards) {
        if (card->property("selected").toBool())
            return card->property("toolId").toString();
    }
    return QString();
}

bool installFixture(SchemeStore &store,
                    ToolsDialog &dialog,
                    const QVector<ToolConfig> &configs,
                    const QMap<QString, ToolPreviewSnapshot> &snapshots,
                    QString *error)
{
    store.setToolConfigs(configs, snapshots);
    if (!store.saveCurrentScheme(error))
        return false;
    dialog.setInitialToolState(configs, snapshots);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QTemporaryDir workspace;
    if (!workspace.isValid())
        return fail(QStringLiteral("temporary workspace creation failed"));

    QFile marker(QDir(workspace.path()).filePath(QStringLiteral("qt_ui_test.pro")));
    if (!marker.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("project marker creation failed"));
    marker.close();
    if (!QDir::setCurrent(workspace.path()))
        return fail(QStringLiteral("could not enter temporary workspace"));

    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error))
        return fail(QStringLiteral("scheme initialization failed: %1").arg(error));

    MainWindow mainWindow;
    ToolsDialog dialog(&mainWindow);
    QToolButton *copyButton = button(dialog, "copyToolButton");
    QToolButton *deleteButton = button(dialog, "deleteToolButton");
    QToolButton *deleteAllButton = button(dialog, "deleteAllToolsButton");
    if (!copyButton || !deleteButton || !deleteAllButton)
        return fail(QStringLiteral("toolbar buttons not found"));
    if (copyButton->toolTip() != QStringLiteral("复制工具")
            || deleteButton->toolTip() != QStringLiteral("删除选中工具")
            || deleteAllButton->toolTip() != QStringLiteral("删除所有工具")) {
        return fail(QStringLiteral("toolbar tooltips mismatch"));
    }
    if (copyButton->icon().isNull() || deleteButton->icon().isNull()
            || deleteAllButton->icon().isNull()) {
        return fail(QStringLiteral("toolbar icon missing"));
    }
    if (copyButton->isEnabled() || deleteButton->isEnabled() || deleteAllButton->isEnabled())
        return fail(QStringLiteral("empty-list toolbar actions must be disabled"));

    const ToolConfig producer = makeTool(QStringLiteral("producer-a"),
                                         QStringLiteral("模板定位"),
                                         ToolType::TemplateLocation,
                                         ToolCategory::Location);
    const ToolConfig dependent = makeDependentTool(QStringLiteral("dependent-b"),
                                                   producer.toolId);
    QMap<QString, ToolPreviewSnapshot> snapshots;
    snapshots.insert(producer.toolId, makeSnapshot(producer));
    if (!installFixture(store, dialog, {producer, dependent}, snapshots, &error))
        return fail(QStringLiteral("fixture save failed: %1").arg(error));
    if (!copyButton->isEnabled() || !deleteButton->isEnabled() || !deleteAllButton->isEnabled())
        return fail(QStringLiteral("populated-list toolbar actions must be enabled"));

    copyButton->click();
    if (dialog.toolConfigs().size() != 3)
        return fail(QStringLiteral("copy did not add a tool"));
    const ToolConfig copied = dialog.toolConfigs().at(1);
    if (copied.toolId.isEmpty() || copied.toolId == producer.toolId
            || copied.displayName != producer.displayName
            || copied.params != producer.params) {
        return fail(QStringLiteral("copied config identity/content mismatch"));
    }
    if (selectedToolId(dialog) != copied.toolId)
        return fail(QStringLiteral("copied tool was not selected"));
    const ToolPreviewSnapshot copiedSnapshot = dialog.referencePreviewSnapshots().value(copied.toolId);
    if (!copiedSnapshot.valid || copiedSnapshot.toolId != copied.toolId
            || copiedSnapshot.result.toolId != copied.toolId) {
        return fail(QStringLiteral("copied snapshot identity mismatch"));
    }
    if (dialog.toolConfigs().at(2).params != dependent.params)
        return fail(QStringLiteral("copy unexpectedly rewrote downstream binding"));
    if (store.currentScheme().toolConfigs.size() != 3)
        return fail(QStringLiteral("copy was not persisted"));
    QTableWidget *mainToolsTable = mainWindow.findChild<QTableWidget *>(
                QStringLiteral("toolsTableWidget"));
    if (!mainToolsTable || mainToolsTable->rowCount() != 3)
        return fail(QStringLiteral("saved tools were not applied to MainWindow immediately"));

    if (!selectTool(dialog, 0))
        return fail(QStringLiteral("could not select producer"));
    answerNextMessage(QMessageBox::Ok);
    deleteButton->click();
    if (dialog.toolConfigs().size() != 3
            || !g_lastMessageText.contains(QStringLiteral("依赖工具"))) {
        return fail(QStringLiteral("referenced producer deletion was not blocked"));
    }

    if (!selectTool(dialog, 1))
        return fail(QStringLiteral("could not select copied tool"));
    answerNextMessage(QMessageBox::No);
    deleteButton->click();
    if (dialog.toolConfigs().size() != 3)
        return fail(QStringLiteral("cancelled single deletion changed list"));
    answerNextMessage(QMessageBox::Yes);
    deleteButton->click();
    if (dialog.toolConfigs().size() != 2
            || dialog.referencePreviewSnapshots().contains(copied.toolId)
            || selectedToolId(dialog) != dependent.toolId) {
        return fail(QStringLiteral("confirmed middle deletion state mismatch"));
    }

    answerNextMessage(QMessageBox::No);
    deleteAllButton->click();
    if (dialog.toolConfigs().size() != 2)
        return fail(QStringLiteral("cancelled delete-all changed list"));
    answerNextMessage(QMessageBox::Yes);
    deleteAllButton->click();
    if (!dialog.toolConfigs().isEmpty() || !dialog.referencePreviewSnapshots().isEmpty()
            || copyButton->isEnabled() || deleteButton->isEnabled() || deleteAllButton->isEnabled()) {
        return fail(QStringLiteral("delete-all final state mismatch"));
    }

    const SchemeState reloadedAfterDeleteAll = store.loadScheme(
                store.currentScheme().schemeId, &error);
    if (!error.isEmpty() || !reloadedAfterDeleteAll.toolConfigs.isEmpty()
            || !reloadedAfterDeleteAll.referencePreviewSnapshots.isEmpty()) {
        return fail(QStringLiteral("delete-all reload mismatch: %1").arg(error));
    }

    const ToolConfig first = makeTool(QStringLiteral("first"),
                                      QStringLiteral("工具1"),
                                      ToolType::TemplateLocation,
                                      ToolCategory::Location);
    const ToolConfig second = makeTool(QStringLiteral("second"),
                                       QStringLiteral("工具2"),
                                       ToolType::TemplateLocation,
                                       ToolCategory::Location);
    const ToolConfig third = makeTool(QStringLiteral("third"),
                                      QStringLiteral("工具3"),
                                      ToolType::TemplateLocation,
                                      ToolCategory::Location);
    if (!installFixture(store, dialog, {first, second, third}, {}, &error)
            || !selectTool(dialog, 2)) {
        return fail(QStringLiteral("last-item fixture setup failed: %1").arg(error));
    }
    answerNextMessage(QMessageBox::Yes);
    deleteButton->click();
    if (dialog.toolConfigs().size() != 2 || selectedToolId(dialog) != second.toolId)
        return fail(QStringLiteral("deleting last item did not select new last item"));

    if (!installFixture(store, dialog, {first}, {}, &error))
        return fail(QStringLiteral("single-item fixture save failed: %1").arg(error));
    answerNextMessage(QMessageBox::Yes);
    deleteButton->click();
    if (!dialog.toolConfigs().isEmpty() || !selectedToolId(dialog).isEmpty()
            || copyButton->isEnabled() || deleteButton->isEnabled() || deleteAllButton->isEnabled()) {
        return fail(QStringLiteral("deleting only item did not restore empty state"));
    }

    const ToolConfig rollbackTool = makeTool(QStringLiteral("rollback-tool"),
                                             QStringLiteral("回滚工具"),
                                             ToolType::TemplateLocation,
                                             ToolCategory::Location);
    if (!installFixture(store, dialog, {rollbackTool}, {}, &error))
        return fail(QStringLiteral("rollback fixture save failed: %1").arg(error));
    const QString schemeDir = store.currentScheme().schemeDir;
    if (!QFile::setPermissions(schemeDir,
                               QFileDevice::ReadOwner | QFileDevice::ExeOwner)) {
        return fail(QStringLiteral("could not inject save failure"));
    }
    answerNextMessage(QMessageBox::Ok);
    copyButton->click();
    QFile::setPermissions(schemeDir,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (dialog.toolConfigs().size() != 1
            || dialog.toolConfigs().first().toolId != rollbackTool.toolId
            || store.currentScheme().toolConfigs.size() != 1
            || store.currentScheme().toolConfigs.first().toolId != rollbackTool.toolId) {
        return fail(QStringLiteral("save failure did not roll back dialog/store state"));
    }

    std::cout << "tools_dialog_actions_smoke: PASS" << std::endl;
    return 0;
}
