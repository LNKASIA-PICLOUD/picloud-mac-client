/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once
#include <QtQuickWidgets/QQuickWidget>

class QUrl;

#define OC_DECLARE_WIDGET_FOCUS                                                                                                                                \
public:                                                                                                                                                        \
    Q_INVOKABLE void focusNext()                                                                                                                               \
    {                                                                                                                                                          \
        focusNextChild();                                                                                                                                      \
    }                                                                                                                                                          \
    Q_INVOKABLE void focusPrevious()                                                                                                                           \
    {                                                                                                                                                          \
        focusPreviousChild();                                                                                                                                  \
    }                                                                                                                                                          \
                                                                                                                                                               \
Q_SIGNALS:                                                                                                                                                     \
    void focusFirst();                                                                                                                                         \
    void focusLast();                                                                                                                                          \
                                                                                                                                                               \
private:

namespace OCC::QmlUtils {
class OCQuickWidget : public QQuickWidget
{
    Q_OBJECT
public:
    using QQuickWidget::QQuickWidget;

Q_SIGNALS:
    void focusFirst();
    void focusLast();

protected:
    void focusInEvent(QFocusEvent *event) override;
};

void initQuickWidget(OCQuickWidget *widget, const QUrl &src, QObject *ocContext);
}
