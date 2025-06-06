// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "button_box_dialog.h"

#include <QtCore/QEvent>
#include <QtWidgets/QPushButton>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/custom_style.h>

QnButtonBoxDialog::QnButtonBoxDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_clickedButton(QDialogButtonBox::NoButton),
    m_readOnly(false)
{
}

QnButtonBoxDialog::~QnButtonBoxDialog()
{
}

void QnButtonBoxDialog::setButtonBox(QDialogButtonBox *buttonBox) {
    if(m_buttonBox.data() == buttonBox)
        return;

    if(m_buttonBox)
        m_buttonBox->disconnect(this);

    m_buttonBox = buttonBox;

    if(m_buttonBox)
    {
        connect(
            m_buttonBox, &QDialogButtonBox::clicked, this, &QnButtonBoxDialog::atButtonBoxClicked);
        connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QnButtonBoxDialog::accept);
        connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QnButtonBoxDialog::reject);

        if (QAbstractButton *okButton = m_buttonBox->button(QDialogButtonBox::Ok))
            setAccentStyle(okButton);
    }
}

bool QnButtonBoxDialog::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QEvent::Polish)
        initializeButtonBox();

    return result;
}

QDialogButtonBox* QnButtonBoxDialog::buttonBox() const {
    return m_buttonBox.data();
}

void QnButtonBoxDialog::initializeButtonBox() {
    if(m_buttonBox)
        return; /* Already initialized with a direct call to setButtonBox in derived class's constructor. */

    QList<QDialogButtonBox *> buttonBoxes = findChildren<QDialogButtonBox *>(QString(), Qt::FindDirectChildrenOnly);
    NX_ASSERT(buttonBoxes.size() == 1, "Invalid buttonBox count");

    // Trying desperately in release builds.
    if (buttonBoxes.isEmpty())
        buttonBoxes = findChildren<QDialogButtonBox *>(QString(), Qt::FindChildrenRecursively);

    if (buttonBoxes.size() != 1)
        return;

    setButtonBox(buttonBoxes[0]);
}

void QnButtonBoxDialog::atButtonBoxClicked(QAbstractButton* button)
{
    if (m_buttonBox)
        m_clickedButton = m_buttonBox.data()->standardButton(button);
    buttonBoxClicked(button);
    buttonBoxClicked(m_clickedButton);
}

void QnButtonBoxDialog::buttonBoxClicked(QAbstractButton* /*button*/)
{
}

void QnButtonBoxDialog::buttonBoxClicked(QDialogButtonBox::StandardButton /*button*/)
{
}

bool QnButtonBoxDialog::isReadOnly() const
{
    return m_readOnly;
}

void QnButtonBoxDialog::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    m_readOnly = readOnly;
    setReadOnlyInternal();
}

void QnButtonBoxDialog::setReadOnlyInternal()
{
    if (!m_buttonBox || !m_readOnly)
        return;

    if (auto button = m_buttonBox->button(QDialogButtonBox::Ok))
        button->setFocus();
}
