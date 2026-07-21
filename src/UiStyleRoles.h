#ifndef UISTYLEROLES_H
#define UISTYLEROLES_H

#include <QAbstractItemView>
#include <QComboBox>
#include <QString>
#include <QStyle>
#include <QWidget>

// Public semantic style roles. Use these helpers instead of dialog-specific
// style sheets so popup widgets keep their theme after Qt reparents them.
namespace UiStyleRoles {

inline QString lightComboPopupStyleSheet()
{
    return QStringLiteral(
        "QAbstractItemView{background:#ffffff;color:#0f172a;"
        "border:2px solid #60a5fa;border-radius:3px;outline:0;"
        "font-size:18px;font-weight:600;"
        "selection-background-color:#ffedd5;selection-color:#9a3412;}"
        "QAbstractItemView::item{min-height:38px;padding:4px 10px;"
        "background:#ffffff;color:#0f172a;}"
        "QAbstractItemView::item:hover{background:#eff6ff;color:#0f172a;}"
        "QAbstractItemView::item:selected{background:#ffedd5;color:#9a3412;}"
        "QAbstractItemView::item:disabled{background:#f1f5f9;color:#64748b;}");
}

inline void repolish(QWidget *widget)
{
    if (!widget || !widget->style())
        return;

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

inline void applyLightComboBox(QComboBox *comboBox)
{
    if (!comboBox)
        return;

    comboBox->setProperty("uiRole", QStringLiteral("lightField"));
    QAbstractItemView *popupView = comboBox->view();
    if (popupView) {
        popupView->setObjectName(QStringLiteral("sharedLightComboPopup"));
        popupView->setProperty("uiRole", QStringLiteral("lightComboPopup"));
        // QComboBoxPrivateContainer is a detached popup on some Qt/platform
        // combinations and does not reliably inherit the application QSS.
        // Keep this override centralized here instead of duplicating it in
        // individual dialogs.
        popupView->setStyleSheet(lightComboPopupStyleSheet());

        QWidget *popupViewport = popupView->viewport();
        if (popupViewport) {
            popupViewport->setObjectName(
                        QStringLiteral("sharedLightComboPopupViewport"));
            popupViewport->setProperty(
                        "uiRole", QStringLiteral("lightComboPopupViewport"));
            repolish(popupViewport);
        }

        QWidget *popupContainer = popupView->parentWidget();
        if (popupContainer && popupContainer != comboBox) {
            popupContainer->setObjectName(
                        QStringLiteral("sharedLightComboPopupContainer"));
            popupContainer->setProperty(
                        "uiRole", QStringLiteral("lightComboPopupContainer"));
            repolish(popupContainer);
        }

        repolish(popupView);
    }
    repolish(comboBox);
}

inline void applyStatusTone(QWidget *widget, const QString &tone)
{
    if (!widget)
        return;
    widget->setProperty("statusTone", tone.trimmed().toLower());
    repolish(widget);
}

} // namespace UiStyleRoles

#endif // UISTYLEROLES_H
