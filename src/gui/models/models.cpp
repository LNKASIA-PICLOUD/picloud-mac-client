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
#include "models.h"

#include <QApplication>
#include <QItemSelectionRange>
#include <QTextStream>
#include <QSortFilterProxyModel>
#include <QMenu>

#include <functional>

QString OCC::Models::formatSelection(const QModelIndexList &items, int dataRole)
{
    if (items.isEmpty()) {
        return {};
    }
    const auto columns = items.first().model()->columnCount();
    QString out;
    QTextStream stream(&out);

    QString begin;
    QString end;

    const auto iterate = [columns](const std::function<void(int)> &f) {
        if (qApp->layoutDirection() != Qt::RightToLeft) {
            for (int c = 0; c < columns; ++c) {
                f(c);
            }
        } else {
            for (int c = columns - 1; c >= 0; --c) {
                f(c);
            }
        }
    };
    if (qApp->layoutDirection() == Qt::RightToLeft) {
        stream << right;
        begin = QLatin1Char(',');
    } else {
        stream << left;
        end = QLatin1Char(',');
    }

    iterate([&](int c) {
        const auto width = items.first().model()->headerData(c, Qt::Horizontal, StringFormatWidthRole).toInt();
        Q_ASSERT(width);
        stream << begin
               << qSetFieldWidth(width)
               << items.first().model()->headerData(c, Qt::Horizontal).toString()
               << qSetFieldWidth(0)
               << end;
    });
    stream << endl;
    for (const auto &index : items) {
        iterate([&](int c) {
            const auto &child = index.siblingAtColumn(c);
            stream << begin
                   << qSetFieldWidth(child.model()->headerData(c, Qt::Horizontal, StringFormatWidthRole).toInt())
                   << child.data(dataRole).toString()
                   << qSetFieldWidth(0)
                   << end;
        });
        stream << endl;
    }
    return out;
}

void OCC::Models::displayFilterDialog(const QStringList &candidates, QSortFilterProxyModel *model, int column, int role, QWidget *parent)
{
    auto menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(qApp->translate("OCC::Models", "Filter by"));
    menu->addSeparator();

    const auto currentFilter = model->filterRegExp().pattern();
    auto addAction = [=](const QString &s, const QString &filter) {
        auto action = menu->addAction(s, parent, [=]() {
            model->setFilterRole(role);
            model->setFilterKeyColumn(column);
            model->setFilterFixedString(filter);
        });
        action->setCheckable(true);
        if (currentFilter == filter) {
            action->setChecked(true);
        }
    };


    addAction(qApp->translate("OCC::Models", "No filter"), QString());

    for (const auto &c : candidates) {
        addAction(c, c);
    }
    menu->popup(QCursor::pos());
}
