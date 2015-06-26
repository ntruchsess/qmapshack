/**********************************************************************************************
    Copyright (C) 2014-2015 Oliver Eichler oliver.eichler@gmx.de

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************************/

#include "CMainWindow.h"
#include "gis/CGisWidget.h"
#include "gis/CGisWidget.h"
#include "gis/IGisLine.h"
#include "gis/prj/IGisProject.h"
#include "gis/rte/CCreateRouteFromWpt.h"
#include "gis/rte/CGisItemRte.h"
#include "gis/wpt/CGisItemWpt.h"


#include <QtWidgets>
#include <proj_api.h>

CCreateRouteFromWpt::CCreateRouteFromWpt(const QList<IGisItem::key_t> &keys, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    foreach(const IGisItem::key_t& key, keys)
    {
        CGisItemWpt * wpt = dynamic_cast<CGisItemWpt*>(CGisWidget::self().getItemByKey(key));
        if(wpt == 0)
        {
            continue;
        }

        QListWidgetItem * item = new QListWidgetItem(listWidget);
        item->setText(wpt->getName());
        item->setIcon(wpt->getIcon());
        item->setToolTip(wpt->getInfo());
        item->setData(Qt::UserRole, QPointF(wpt->getPosition()*DEG_TO_RAD));
    }

    connect(listWidget, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
    connect(toolUp, SIGNAL(clicked()), this, SLOT(slotUp()));
    connect(toolDown, SIGNAL(clicked()), this, SLOT(slotDown()));
}

CCreateRouteFromWpt::~CCreateRouteFromWpt()
{
}

void CCreateRouteFromWpt::accept()
{
    QString name = QInputDialog::getText(&CMainWindow::self(), QObject::tr("Edit name..."), QObject::tr("Enter new route name."), QLineEdit::Normal, "");
    if(name.isEmpty())
    {
        return;
    }

    IGisProject * project = CGisWidget::self().selectProject();
    if(project == 0)
    {
        return;
    }

    SGisLine points;
    for(int i = 0; i < listWidget->count(); i++)
    {
        QListWidgetItem * item = listWidget->item(i);
        points << IGisLine::point_t(item->data(Qt::UserRole).toPointF());
    }

    CGisItemRte* rte = new CGisItemRte(points,name, project, NOIDX);
    rte->calc();

    QDialog::accept();
}

void CCreateRouteFromWpt::slotSelectionChanged()
{
    QListWidgetItem * item = listWidget->currentItem();
    if(item != 0)
    {
        int row = listWidget->row(item);
        toolUp->setEnabled(row != 0);
        toolDown->setEnabled(row != (listWidget->count() - 1));
    }
    else
    {
        toolUp->setEnabled(false);
        toolDown->setEnabled(false);
    }
}

void CCreateRouteFromWpt::slotUp()
{
    QListWidgetItem * item = listWidget->currentItem();
    if(item)
    {
        int row = listWidget->row(item);
        if(row == 0)
        {
            return;
        }
        listWidget->takeItem(row);
        row = row - 1;
        listWidget->insertItem(row,item);
        listWidget->setCurrentItem(item);
    }
}

void CCreateRouteFromWpt::slotDown()
{
    QListWidgetItem * item = listWidget->currentItem();
    if(item)
    {
        int row = listWidget->row(item);
        if(row == (listWidget->count() - 1))
        {
            return;
        }
        listWidget->takeItem(row);
        row = row + 1;
        listWidget->insertItem(row,item);
        listWidget->setCurrentItem(item);
    }
}