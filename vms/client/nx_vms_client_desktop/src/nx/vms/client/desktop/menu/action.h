// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QAction>

#include <nx/vms/client/desktop/window_context_aware.h>

#include "action_conditions.h"
#include "action_fwd.h"
#include "action_types.h"
#include "actions.h"

namespace nx::vms::client::desktop {
namespace menu {

/**
 * Action class that hooks into actions infrastructure to correctly check conditions and provide
 * proper action parameters even if it was triggered with a hotkey.
 */
class Action: public QAction, public WindowContextAware
{
    Q_OBJECT

public:
    Action(IDType id, WindowContext* context, QObject* parent = nullptr);
    virtual ~Action() override;

    /**
     * Identifier of this action.
     */
    IDType id() const;

    /**
     * Scope of this action.
     */
    ActionScopes scope() const;

    /**
     * Possible types of this action's default parameter.
     */
    ActionParameterTypes defaultParameterTypes() const;

    /**
     * @param target Action parameter key.
     * @return Permissions that are required for the provided parameter.
     */
    Qn::Permissions requiredTargetPermissions(int target = -1) const;

    /**
     * Set permissions required for some specific resource, passed as target.
     * @param target Action parameter key.
     * @param requiredPermissions Permissions required for the provided parameter.
     */
    void setRequiredTargetPermissions(int target, Qn::Permissions requiredPermissions);

    /**
     * Set permissions required for the target resources.
     */
    void setRequiredTargetPermissions(Qn::Permissions requiredPermissions);

    /**
     * Set global permissions that the current user must have.
     */
    void setRequiredGlobalPermission(GlobalPermission requiredPermissions);

    bool isPowerUserRequired() const;
    void setPowerUserRequired(bool value = true);

    ClientModes mode() const;
    void setMode(ClientModes mode);

    ActionFlags flags() const;
    void setFlags(ActionFlags flags);

    Qn::ButtonAccent accent() const;
    void setAccent(Qn::ButtonAccent value);

    /**
     * Default text of this action.
     */
    const QString& normalText() const;

    /**
     * Set default text of this action.
     */
    void setNormalText(const QString& normalText);

    /**
     * Text for this action that is to be used when it is toggled.
     */
    const QString& toggledText() const;

    /**
     * Text for this action that is to be used when it is toggled. If empty, default text will be
     * used.
     */
    void setToggledText(const QString& toggledText);

    /**
     * Text for this action that is to be used when it is pulled into the enclosing menu.
     */
    const QString& pulledText() const;

    /**
     * Set text for this action that is to be used when it is pulled into the enclosing menu. If
     * empty, default text will be used.
     */
    void setPulledText(const QString& pulledText);

    /**
     * Condition associated with this action, of nullptr if none.
     */
    bool hasCondition() const;

    /**
     * Set condition for this action.
     */
    void setCondition(ConditionWrapper&& condition);

    FactoryPtr childFactory() const;
    void setChildFactory(const FactoryPtr& childFactory);

    TextFactoryPtr textFactory() const;
    void setTextFactory(const TextFactoryPtr& textFactory);

    /**
     * Child actions. These action will appear in a submenu for this action.
     */
    const QList<Action*>& children() const;

    void addChild(Action* action);

    void removeChild(Action* action);

    QString disabledToolTip() const;
    void setDisabledToolTip(const QString& toolTip);

    QString toolTipFormat() const;
    void setToolTipFormat(const QString& toolTipFormat);

    /**
     * \param scope                     Scope in which action is to be executed.
     * \param parameters                Parameters for action execution.
     * \returns                         Action visibility that determines whether
     *                                  action can be executed and how it will
     *                                  appear in context menu.
     */
    ActionVisibility checkCondition(ActionScopes scope, const Parameters& parameters) const;

    void addConditionalText(ConditionWrapper&& condition, const QString& text);

    /**
     * \returns true if there is at least one conditional text
     */
    bool hasConditionalTexts() const;

    /**
     * \param parameters                Parameters for action execution.
     * \returns                         New text if condition is executed;
     *                                  empty string otherwise.
     */
    QString checkConditionalText(const Parameters& parameters) const;

protected:
    virtual bool event(QEvent* event) override;

private slots:
    void updateText();
    void updateToolTip(bool notify);
    void updateToolTipSilent();

private:
    QString defaultToolTipFormat() const;

private:
    const IDType m_id;
    ActionFlags m_flags;
    Qn::ButtonAccent m_accent{Qn::ButtonAccent::NoAccent};
    ClientModes m_mode;
    QHash<int, Qn::Permissions> m_targetPermissions;
    GlobalPermission m_globalPermission;
    bool m_powerUserRequired = false;
    QString m_normalText, m_toggledText, m_pulledText;
    QString m_disabledToolTip;
    QString m_toolTipFormat, m_toolTipMarker;
    ConditionWrapper m_condition;
    FactoryPtr m_childFactory;
    TextFactoryPtr m_textFactory;

    QList<Action*> m_children;

    struct ConditionalText
    {
        ConditionWrapper condition;
        QString text;
        ConditionalText() = delete;
        ConditionalText(ConditionalText&& conditionalText);
        ConditionalText(ConditionWrapper&& condition, const QString& text);
        ~ConditionalText();
    };
    std::vector<ConditionalText> m_conditionalTexts;
};

} // namespace menu
} // namespace nx::vms::client::desktop
