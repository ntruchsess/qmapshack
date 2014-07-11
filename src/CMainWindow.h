/**********************************************************************************************
    Copyright (C) 2014 Oliver Eichler oliver.eichler@gmx.de

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

#ifndef CMAINWINDOW_H
#define CMAINWINDOW_H

#include <QMainWindow>
#include "ui_IMainWindow.h"

class CMapList;
class CDemList;
class QLabel;
class CGisWidget;

class CMainWindow : public QMainWindow, private Ui::IMainWindow
{
    Q_OBJECT
    public:
        static CMainWindow& self(){return *pSelf;}
        virtual ~CMainWindow();

        void addMapList(CMapList *list, const QString& name);
        void addDemList(CDemList *list, const QString& name);

        bool isScaleVisible();
        bool isGridVisible();
        bool isNight();
        bool isPOIText();
        bool isMapToolTip();
        bool flipMouseWheel();
        const QFont& getMapFont(){return mapFont;}

    private slots:
        void slotAbout();
        void slotHelp();
        void slotAddCanvas();
        void slotTabCloseRequest(int i);
        void slotCurrentTabCanvas(int i);
        void slotCurrentTabMaps(int i);
        void slotCurrentTabDem(int i);
        void slotMousePosition(const QPointF& pos, qreal ele);
        void slotUpdateCurrentWidget();
        void slotSetupMapFont();
        void slotSetupGrid();
        void slotSetupMapPath();
        void slotSetupDemPath();
        void slotSetupMapWks();
        void slotLoadGISData();
        void slotSaveGISData();

    private:
        friend int main(int argc, char ** argv);
        CMainWindow();

        static CMainWindow * pSelf;

        /// status bar label
        QLabel * lblPosWGS84;
        QLabel * lblElevation;
        QLabel * lblPosGrid;

        QFont mapFont;

        CGisWidget * gisWidget;
};

#endif //CMAINWINDOW_H

