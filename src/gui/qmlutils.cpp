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

#include "gui/qmlutils.h"

#include "resources/resources.h"

#include <QMessageBox>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>

void OCC::QmlUtils::initQuickWidget(OCQuickWidget *widget, const QUrl &src, QObject *ocContext)
{
    widget->rootContext()->setContextProperty(QStringLiteral("ocContext"), ocContext);
    widget->engine()->addImageProvider(QStringLiteral("ownCloud"), new OCC::Resources::CoreImageProvider());
    widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    widget->setSource(src);
    if (!widget->errors().isEmpty()) {
        auto box = new QMessageBox(QMessageBox::Critical, QStringLiteral("QML Error"), QDebug::toString(widget->errors()));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->exec();
        qFatal("A qml error occured %s", qPrintable(QDebug::toString(widget->errors())));
    }

    // string based connects as they are provided by OC_DECLARE_WIDGET_FOCUS and not inherited
    QObject::connect(widget, SIGNAL(focusFirst()), ocContext, SIGNAL(focusFirst()));
    QObject::connect(widget, SIGNAL(focusLast()), ocContext, SIGNAL(focusLast()));
}

void OCC::QmlUtils::OCQuickWidget::focusInEvent(QFocusEvent *event)
{
    switch (event->reason()) {
    case Qt::TabFocusReason:
        Q_EMIT focusFirst();
        break;
    case Qt::BacktabFocusReason:
        Q_EMIT focusLast();
        break;
    default:
        break;
    }
    QQuickWidget::focusInEvent(event);
}
bool OCC::QmlUtils::OCQuickWidget::event(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        rootObject()->setEnabled(isEnabled());
    }

    return QQuickWidget::event(event);
}
