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

#include "gis/trk/CDetailsTrk.h"
#include "gis/trk/CKnownExtension.h"
#include "gis/trk/CPropertyTrk.h"
#include "gis/trk/filter/CFilterDelete.h"
#include "gis/trk/filter/CFilterDouglasPeuker.h"
#include "gis/trk/filter/CFilterInvalid.h"
#include "gis/trk/filter/CFilterMedian.h"
#include "gis/trk/filter/CFilterNewDate.h"
#include "gis/trk/filter/CFilterObscureDate.h"
#include "gis/trk/filter/CFilterOffsetElevation.h"
#include "gis/trk/filter/CFilterReplaceElevation.h"
#include "gis/trk/filter/CFilterReset.h"
#include "gis/trk/filter/CFilterSpeed.h"
#include "gis/trk/filter/CFilterSplitSegment.h"
#include "helpers/CLinksDialog.h"
#include "helpers/CSettings.h"
#include "plot/CPlot.h"
#include "plot/CPlotProfile.h"
#include "units/IUnit.h"
#include "widgets/CTextEditWidget.h"

#include <QtWidgets>
#include <proj_api.h>

/* base case: add the filter specified in template parameter */
template<typename filter>
static void addFilters(QTreeWidgetItem *itemGroup, CGisItemTrk& trk)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(itemGroup);
    itemGroup->treeWidget()->setItemWidget(item, /* column = */ 0, new filter(trk, itemGroup->treeWidget()));
}

template<typename filter1, typename filter2, typename ... remainingFilters>
static void addFilters(QTreeWidgetItem *itemGroup, CGisItemTrk& trk)
{
    addFilters<filter1>(itemGroup, trk);
    addFilters<filter2, remainingFilters ...>(itemGroup, trk);
}

template<typename ... filters>
static void addFilterGroup(QTreeWidget *widget, CGisItemTrk& trk, const QString &groupText, const QString &groupIcon)
{
    QTreeWidgetItem *itemGroup = new QTreeWidgetItem(widget);
    itemGroup->setIcon(/* column = */ 0, QIcon(groupIcon));
    itemGroup->setText(/* column = */ 0, groupText);

    addFilters<filters ...>(itemGroup, trk);
}

CDetailsTrk::CDetailsTrk(CGisItemTrk& trk, QWidget *parent)
    : QWidget(parent)
    , INotifyTrk(CGisItemTrk::eVisualDetails)
    , trk(trk)
{
    setupUi(this);

    QPixmap icon(16,8);

    for(int i=0; i < TRK_N_COLORS; ++i)
    {
        icon.fill(IGisItem::colorMap[i].color);
        comboColor->addItem(icon, IGisItem::colorMap[i].name, IGisItem::colorMap[i].color);
    }

    widgetColorLayout->setAlignment(Qt::AlignTop);

    const CActivityTrk::desc_t* actDesc = CActivityTrk::getActivityDescriptors();
    for(int i = 0; !actDesc[i].name.isEmpty(); ++i)
    {
        const CActivityTrk::desc_t& desc = actDesc[i];
        QCheckBox * check = new QCheckBox(this);
        check->setText(desc.name);
        check->setIcon(QIcon(desc.iconLarge));
        check->setProperty("flag",   desc.flag);
        check->setProperty("name",   desc.name);
        check->setProperty("symbol", desc.iconLarge);
        check->setObjectName("check" + desc.objName);

        connect(check, &QCheckBox::clicked, this, &CDetailsTrk::slotActivitySelected);

        layoutActivities->addWidget(check, i%8, i>>3);
    }
    layoutActivities->addItem(new QSpacerItem(0,0,QSizePolicy::Maximum, QSizePolicy::MinimumExpanding),8,0);

    updateData();

    plot1 = new CPlotProfile(&trk, trk.limitsGraph1, IPlot::eModeNormal, this);
    plot2 = new CPlot(&trk, trk.limitsGraph2, this);
    plot3 = new CPlot(&trk, trk.limitsGraph3, this);

    for(IPlot *plot : { static_cast<IPlot*>(plot1), static_cast<IPlot*>(plot2), static_cast<IPlot*>(plot3) })
    {
        plot->setMinimumSize(QSize(0, 100));
        plot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        plot->show();
        layoutPlot->addWidget(plot);
    }

    if(trk.isOnDevice())
    {
        toolLock->setDisabled(true);
    }

    addFilterGroup<CFilterDouglasPeuker, CFilterInvalid, CFilterReset, CFilterDelete>
        (treeFilter, trk, tr("Reduce visible track points"), "://icons/48x48/PointHide.png");

    addFilterGroup<CFilterMedian, CFilterReplaceElevation, CFilterOffsetElevation>
        (treeFilter, trk, tr("Change elevation of track points"), "://icons/48x48/SetEle.png");

    addFilterGroup<CFilterNewDate, CFilterObscureDate, CFilterSpeed>
        (treeFilter, trk, tr("Change timestamp of track points"), "://icons/48x48/Time.png");

    addFilterGroup<CFilterSplitSegment>
        (treeFilter, trk, tr("Cut track into pieces"), "://icons/48x48/TrkCut.png");


    SETTINGS;
    cfg.beginGroup("TrackDetails");
    checkGraph1->setChecked(cfg.value("showGraph1", true).toBool());
    checkGraph2->setChecked(cfg.value("showGraph2", true).toBool());
    checkGraph3->setChecked(cfg.value("showGraph3", true).toBool());
    splitter->restoreState (cfg.value("splitterSizes").toByteArray());
    treeWidget->header()->restoreState(cfg.value("trackPointListState").toByteArray());
    tabWidget->setCurrentIndex(cfg.value("visibleTab", 0).toInt());
    cfg.endGroup();

    connect(checkGraph1,      &QCheckBox::clicked,                 this, &CDetailsTrk::slotShowPlots);
    connect(checkGraph2,      &QCheckBox::clicked,                 this, &CDetailsTrk::slotShowPlots);
    connect(checkGraph3,      &QCheckBox::clicked,                 this, &CDetailsTrk::slotShowPlots);

    connect(toolLock,         &QToolButton::toggled,               this, &CDetailsTrk::slotChangeReadOnlyMode);
    connect(treeWidget,       &QTreeWidget::itemSelectionChanged,  this, &CDetailsTrk::slotItemSelectionChanged);
    connect(textCmtDesc,      &QTextBrowser::anchorClicked,        this, &CDetailsTrk::slotLinkActivated);

    connect(lineName,         &QLineEdit::textEdited,              this, &CDetailsTrk::slotNameChanged);
    connect(lineName,         &QLineEdit::editingFinished,         this, &CDetailsTrk::slotNameChangeFinished);

    connect(plot1,            &CPlot::sigMouseClickState,          this, &CDetailsTrk::slotMouseClickState);
    connect(plot2,            &CPlot::sigMouseClickState,          this, &CDetailsTrk::slotMouseClickState);
    connect(plot3,            &CPlot::sigMouseClickState,          this, &CDetailsTrk::slotMouseClickState);

    connect(spinLimitHigh,    &CDoubleSpinBox::valueChangedByStep, this, &CDetailsTrk::slotColorLimitHighChanged);
    connect(spinLimitHigh,    &CDoubleSpinBox::editingFinished,    this, &CDetailsTrk::slotColorLimitHighChanged);
    connect(spinLimitLow,     &CDoubleSpinBox::valueChangedByStep, this, &CDetailsTrk::slotColorLimitLowChanged);
    connect(spinLimitLow,     &CDoubleSpinBox::editingFinished,    this, &CDetailsTrk::slotColorLimitLowChanged);

    connect(spinLineWidth,    static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CDetailsTrk::slotLineWidth);
    connect(checkWithArrows,  &QCheckBox::toggled, this, &CDetailsTrk::slotWithArrows);

    connect(toolStyleLimitMax,&QPushButton::clicked, this, &CDetailsTrk::slotLimitHighFromData);
    connect(toolStyleLimitMin,&QPushButton::clicked, this, &CDetailsTrk::slotLimitLowFromData);

    connect(toolUserLineWith, &QToolButton::toggled, this, &CDetailsTrk::slotLineWidthMode);
    connect(toolUserArrow,    &QToolButton::toggled, this, &CDetailsTrk::slotWithArrowsMode);

    connect(listHistory,      &CHistoryListWidget::sigChanged, this, &CDetailsTrk::updateData);

    connect(comboColor,       static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CDetailsTrk::slotColorChanged);
    connect(comboColorSource, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CDetailsTrk::slotColorSourceChanged);
    connect(comboGraph2,      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CDetailsTrk::slotSetupGraph);
    connect(comboGraph3,      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CDetailsTrk::slotSetupGraph);

    setupGraphLimits(trk.limitsGraph1, toolLimitAutoGraph1, toolLimitUsrGraph1, toolLimitSysGraph1, spinMinGraph1, spinMaxGraph1);
    setupGraphLimits(trk.limitsGraph2, toolLimitAutoGraph2, toolLimitUsrGraph2, toolLimitSysGraph2, spinMinGraph2, spinMaxGraph2);
    setupGraphLimits(trk.limitsGraph3, toolLimitAutoGraph3, toolLimitUsrGraph3, toolLimitSysGraph3, spinMinGraph3, spinMaxGraph3);

    // setting up the graph properties will trigger the signals
    // this is good because the signals are connected at this point
    // invoking the slots
    loadGraphSource(comboGraph2, 2, CKnownExtension::internalSpeed);
    loadGraphSource(comboGraph3, 3, CKnownExtension::internalProgress);

    slotShowPlots();
}

CDetailsTrk::~CDetailsTrk()
{
    SETTINGS;
    cfg.beginGroup("TrackDetails");
    cfg.setValue("showGraph1", checkGraph1->isChecked());
    cfg.setValue("showGraph2", checkGraph2->isChecked());
    cfg.setValue("showGraph3", checkGraph3->isChecked());
    cfg.setValue("splitterSizes",       splitter->saveState());
    cfg.setValue("trackPointListState", treeWidget->header()->saveState());
    cfg.setValue("visibleTab",          tabWidget->currentIndex());
    cfg.endGroup();

    saveGraphSource(comboGraph2, 2);
    saveGraphSource(comboGraph3, 3);
}

void CDetailsTrk::slotLimitLowFromData()
{
    qreal min = trk.getMin(trk.getColorizeSource());
    spinLimitLow->setValue(min);
    slotColorLimitLowChanged();
    slotColorLimitHighChanged();
}

void CDetailsTrk::slotLimitHighFromData()
{
    qreal max = trk.getMax(trk.getColorizeSource());
    spinLimitHigh->setValue(max);
    slotColorLimitHighChanged();
    slotColorLimitLowChanged();
}

void CDetailsTrk::setupGraphLimits(CLimit& limit, QToolButton * toolLimitAutoGraph, QToolButton * toolLimitUsrGraph, QToolButton * toolLimitSysGraph, QDoubleSpinBox * spinMinGraph, QDoubleSpinBox * spinMaxGraph)
{
    bool isAutoMode = (limit.getMode() == CLimit::eModeAuto);

    spinMinGraph->setDisabled(isAutoMode);
    spinMinGraph->setSuffix(limit.getUnit());
    spinMinGraph->setValue(limit.getMin());

    spinMaxGraph->setDisabled(isAutoMode);
    spinMaxGraph->setSuffix(limit.getUnit());
    spinMaxGraph->setValue(limit.getMax());

    switch(limit.getMode())
    {
    case CLimit::eModeUser:
        toolLimitUsrGraph->setChecked(true);
        break;

    case CLimit::eModeAuto:
        toolLimitAutoGraph->setChecked(true);
        break;

    case CLimit::eModeSys:
        toolLimitSysGraph->setChecked(true);
        break;
    }

    connect(toolLimitAutoGraph, &QToolButton::toggled, spinMinGraph, &QDoubleSpinBox::setDisabled);
    connect(toolLimitAutoGraph, &QToolButton::toggled, spinMaxGraph, &QDoubleSpinBox::setDisabled);
    connect(toolLimitAutoGraph, &QToolButton::toggled, this, &CDetailsTrk::slotSetLimitModeAuto);
    connect(toolLimitUsrGraph,  &QToolButton::toggled, this, &CDetailsTrk::slotSetLimitModeUser);
    connect(toolLimitSysGraph,  &QToolButton::toggled, this, &CDetailsTrk::slotSetLimitModeSys);

    connect(spinMinGraph, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), &limit, &CLimit::setMin);
    connect(spinMaxGraph, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), &limit, &CLimit::setMax);

    connect(&limit, &CLimit::sigChanged, this, &CDetailsTrk::slotLimitChanged);
}

void CDetailsTrk::loadGraphSource(QComboBox * comboBox, qint32 n, const QString cfgDefault)
{
    const CPropertyTrk * p = trk.getPropertyHandler();

    comboBox->blockSignals(true);
    p->fillComboBox(comboBox);
    comboBox->blockSignals(false);

    // try to restore last graph setup
    // signals are unblocked by now changing the combobox will trigger a graph update
    SETTINGS;
    cfg.beginGroup("TrackDetails");
    int i = comboBox->findData(cfg.value(QString("propGraph%1").arg(n),cfgDefault).toString());
    if(i != NOIDX)
    {
        comboBox->setCurrentIndex(i);
    }
    cfg.endGroup();
}

void CDetailsTrk::saveGraphSource(QComboBox * comboBox, qint32 n)
{
    SETTINGS;
    cfg.beginGroup("TrackDetails");
    if(comboBox->currentIndex() != 0)
    {
        cfg.setValue(QString("propGraph%1").arg(n), comboBox->currentData().toString());
    }
    cfg.endGroup();
}


void CDetailsTrk::updateData()
{
    if(originator)
    {
        return;
    }
    CCanvas::setOverrideCursor(Qt::WaitCursor, "CDetailsTrk::updateData");
    originator = true;

    bool isReadOnly = trk.isReadOnly();

    tabWidget->widget(eTabFilter)->setEnabled(!isReadOnly);
    tabWidget->widget(eTabActivity)->setEnabled(!isReadOnly);

    labelTainted->setVisible(trk.isTainted());

    labelInfo->setText(trk.getInfo(false));
    comboColor->setCurrentIndex(trk.getColorIdx());
    toolLock->setChecked(isReadOnly);

    lineName->setText(trk.getName());
    lineName->setReadOnly(isReadOnly);

    QList<QTreeWidgetItem*> items;
    const CGisItemTrk::trk_t& t = trk.getTrackData();
    foreach (const CGisItemTrk::trkseg_t& seg, t.segs)
    {
        foreach(const CGisItemTrk::trkpt_t& trkpt, seg.pts)
        {
            QString val, unit;

            QTreeWidgetItem * item = new QTreeWidgetItem();
            item->setTextAlignment(eColNum,     Qt::AlignLeft);
            item->setTextAlignment(eColEle,     Qt::AlignRight);
            item->setTextAlignment(eColDelta,   Qt::AlignRight);
            item->setTextAlignment(eColDist,    Qt::AlignRight);
            item->setTextAlignment(eColAscend,  Qt::AlignRight);
            item->setTextAlignment(eColDescend, Qt::AlignRight);
            item->setTextAlignment(eColSpeed,   Qt::AlignRight);

            QBrush b( trkpt.flags & CGisItemTrk::trkpt_t::eHidden ? Qt::gray : Qt::black );
            for(int i = 0; i < eColMax; i++)
            {
                item->setForeground(i, b);
            }

            item->setText(eColNum,QString::number(trkpt.idxTotal));

            item->setText(eColTime, trkpt.time.isValid()
                          ? IUnit::self().datetime2string(trkpt.time, true, QPointF(trkpt.lon, trkpt.lat)*DEG_TO_RAD)
                          : "-"
                          );

            if(trkpt.ele != NOINT)
            {
                IUnit::self().meter2elevation(trkpt.ele, val, unit);
                item->setText(eColEle, tr("%1 %2").arg(val).arg(unit));
            }
            else
            {
                item->setText(eColEle, "-");
            }

            IUnit::self().meter2distance(trkpt.deltaDistance, val, unit);
            item->setText(eColDelta, tr("%1 %2").arg(val).arg(unit));

            IUnit::self().meter2distance(trkpt.distance, val, unit);
            item->setText(eColDist, tr("%1 %2").arg(val).arg(unit));

            if(trkpt.speed != NOFLOAT)
            {
                IUnit::self().meter2speed(trkpt.speed, val, unit);
                item->setText(eColSpeed, tr("%1 %2").arg(val).arg(unit));
            }
            else
            {
                item->setText(eColSpeed, "-");
            }

            item->setText(eColSlope,
                          (trkpt.slope1 != NOFLOAT)
                          ? QString("%1°(%2%)").arg(trkpt.slope1, 2, 'f', 0).arg(trkpt.slope2, 2, 'f', 0)
                          : "-"
                          );

            IUnit::self().meter2elevation(trkpt.ascend, val, unit);
            item->setText(eColAscend, tr("%1 %2").arg(val).arg(unit));
            IUnit::self().meter2elevation(trkpt.descend, val, unit);
            item->setText(eColDescend, tr("%1 %2").arg(val).arg(unit));

            // position
            QString str;
            IUnit::degToStr(trkpt.lon, trkpt.lat, str);
            item->setText(eColPosition,str);

            items << item;
        }
    }

    treeWidget->clear();
    treeWidget->addTopLevelItems(items);
    treeWidget->header()->resizeSections(QHeaderView::ResizeToContents);

    textCmtDesc->document()->clear();
    textCmtDesc->append(IGisItem::createText(isReadOnly, trk.getComment(), trk.getDescription(), trk.getLinks()));
    textCmtDesc->moveCursor (QTextCursor::Start);
    textCmtDesc->ensureCursorVisible();

    quint32 flags = trk.getActivities().getAllFlags();

    const CActivityTrk::desc_t* actDesc = CActivityTrk::getActivityDescriptors();
    for(int i = 0; !actDesc[i].objName.isEmpty(); ++i)
    {
        const CActivityTrk::desc_t& desc = actDesc[i];
        QCheckBox * check = findChild<QCheckBox*>("check" + desc.objName);
        if(nullptr != check)
        {
            check->setChecked((flags & desc.flag) == desc.flag);
        }
    }

    QString str;
    trk.getActivities().printSummary(str);
    labelActivityInfo->setText(str);

    bool hasActivity = 0 != (flags & CGisItemTrk::trkpt_t::eActMask);
    labelActivityHelp->setVisible(!hasActivity);
    labelActivityInfo->setVisible(hasActivity);

    plotTrack->setTrack(&trk);
    listHistory->setupHistory(trk);

    QTabWidget * tabWidget = dynamic_cast<QTabWidget*>(parentWidget() ? parentWidget()->parentWidget() : 0);
    if(nullptr != tabWidget)
    {
        int idx = tabWidget->indexOf(this);
        if(idx != NOIDX)
        {
            setObjectName(trk.getName());
            tabWidget->setTabText(idx, trk.getName());
        }
    }

    spinLineWidth->blockSignals(true);
    spinLineWidth->setValue(trk.lineScale.val().toDouble());
    spinLineWidth->blockSignals(false);
    toolUserLineWith->blockSignals(true);
    toolUserLineWith->setChecked(trk.lineScale.getMode() == CValue::eModeUser);
    toolUserLineWith->blockSignals(false);

    checkWithArrows->blockSignals(true);
    checkWithArrows->setChecked(trk.showArrows.val().toBool());
    checkWithArrows->blockSignals(false);
    toolUserArrow->blockSignals(true);
    toolUserArrow->setChecked(trk.showArrows.getMode() == CValue::eModeUser);
    toolUserArrow->blockSignals(false);

    comboColorSource->blockSignals(true);
    comboColorSource->clear();
    // the first entry `solid color`, it is always available
    comboColorSource->addItem(QIcon("://icons/32x32/CSrcSolid.png"), tr("Solid color"));
    foreach(const QString &key, trk.getExistingDataSources())
    {
        const CKnownExtension &ext = CKnownExtension::get(key);
        QIcon icon(ext.icon);
        comboColorSource->addItem(icon, ext.known ? ext.name : key, key);
    }
    int currentIdx = comboColorSource->findData(trk.getColorizeSource());
    if(-1 == currentIdx)
    {
        currentIdx = 0;
    }
    comboColorSource->setCurrentIndex(currentIdx);
    comboColorSource->blockSignals(false);

    bool enabled = (0 < currentIdx);

    spinLimitLow->setEnabled    (enabled);
    spinLimitHigh->setEnabled   (enabled);
    widgetColorLabel->setEnabled(enabled);
    toolStyleLimitMin->setEnabled(enabled);
    toolStyleLimitMax->setEnabled(enabled);

    if(enabled)
    {
        const CKnownExtension &ext = CKnownExtension::get(comboColorSource->itemData(currentIdx).toString());

        spinLimitLow->blockSignals(true);
        spinLimitLow->setMinimum(ext.minimum);
        spinLimitLow->setMaximum(ext.maximum);
        spinLimitLow->setSuffix (ext.unit);
        spinLimitLow->setValue  (trk.getColorizeLimitLow());
        spinLimitLow->blockSignals(false);

        spinLimitHigh->blockSignals(true);
        spinLimitHigh->setMinimum(ext.minimum);
        spinLimitHigh->setMaximum(ext.maximum);
        spinLimitHigh->setSuffix (ext.unit);
        spinLimitHigh->setValue  (trk.getColorizeLimitHigh());
        spinLimitHigh->blockSignals(false);

        widgetColorLabel->setMinimum(spinLimitLow->value());
        widgetColorLabel->setMaximum(spinLimitHigh->value());
        widgetColorLabel->setUnit(ext.unit);
    }


    // refill comboboxes to select track property to be displayed by graphs
    comboGraph2->blockSignals(true);
    comboGraph3->blockSignals(true);
    loadGraphSource(comboGraph2, 2, CKnownExtension::internalSpeed);
    loadGraphSource(comboGraph3, 3, CKnownExtension::internalProgress);
    comboGraph2->blockSignals(false);
    comboGraph3->blockSignals(false);

    originator = false;
    CCanvas::restoreOverrideCursor("CDetailsTrk::updateData");
}

void CDetailsTrk::setMouseFocus(const CGisItemTrk::trkpt_t * pt)
{
    if(nullptr != pt)
    {
        plotTrack->setMouseFocus(pt->lon, pt->lat);
        labelInfoTrkPt->setText(trk.getInfoTrkPt(*pt));
        labelInfoProgress->setText(trk.getInfoProgress(*pt));
    }
    else
    {
        labelInfoTrkPt->setText("-\n-");
        labelInfoProgress->setText("-\n-");
    }
}

void CDetailsTrk::setMouseRangeFocus(const CGisItemTrk::trkpt_t *pt1, const CGisItemTrk::trkpt_t *pt2)
{
    labelInfoRange->setText( (pt1 && pt2) ? trk.getInfoRange(*pt1, *pt2) : "-\n-" );
}

void CDetailsTrk::setMouseClickFocus(const CGisItemTrk::trkpt_t *pt)
{
    if(nullptr != pt)
    {
        treeWidget->blockSignals(true);
        treeWidget->setCurrentItem(treeWidget->topLevelItem(pt->idxTotal));
        treeWidget->blockSignals(false);
    }
}

void CDetailsTrk::slotMouseClickState(int s)
{
    if(s == IPlot::eMouseClickIdle)
    {
        labelInfoRange->setText("-\n-");
        plot3->setMouseRangeFocus(0, 0);
        plot1->setMouseRangeFocus(0, 0);
        plot2->setMouseRangeFocus(0, 0);
    }
}

void CDetailsTrk::slotNameChanged(const QString &name)
{
    QTabWidget *tabWidget = dynamic_cast<QTabWidget*>(parentWidget() ? parentWidget()->parentWidget() : nullptr);
    if(nullptr != tabWidget)
    {
        int idx = tabWidget->indexOf(this);
        if(idx != NOIDX)
        {
            const QString shownName = name.isEmpty() ? CGisItemTrk::noName : QString(name).replace('&', "&&");
            setObjectName(shownName);
            tabWidget->setTabText(idx, shownName);
        }
    }
}

void CDetailsTrk::slotNameChangeFinished()
{
    lineName->clearFocus();

    const QString& name = lineName->text();
    slotNameChanged(name);

    if(name != trk.getName())
    {
        trk.setName(name);
        updateData();
    }
}


void CDetailsTrk::slotShowPlots()
{
    plot1->setVisible(checkGraph1->isChecked());
    plot2->setVisible(checkGraph2->isChecked());
    plot3->setVisible(checkGraph3->isChecked());
}

void CDetailsTrk::slotColorChanged(int idx)
{
    if(trk.getColorIdx() != idx)
    {
        trk.setColor(idx);
    }
}

void CDetailsTrk::slotColorSourceChanged(int idx)
{
    trk.setColorizeSource(comboColorSource->itemData(idx).toString());
    updateData();
}

void CDetailsTrk::slotColorLimitHighChanged()
{
    const double val = spinLimitHigh->value();
    trk.setColorizeLimitHigh(val);
    widgetColorLabel->setMaximum(val);

    if(spinLimitLow->value() >= val)
    {
        spinLimitLow->setValue(val - .1f);
    }
}

void CDetailsTrk::slotColorLimitLowChanged()
{
    const double val = spinLimitLow->value();
    trk.setColorizeLimitLow(val);
    widgetColorLabel->setMinimum(val);

    if(spinLimitHigh->value() <= val)
    {
        spinLimitHigh->setValue(val + .1f);
    }
}

void CDetailsTrk::slotChangeReadOnlyMode(bool on)
{
    trk.setReadOnlyMode(on);
    updateData();
}


void CDetailsTrk::slotItemSelectionChanged()
{
    QTreeWidgetItem * item = treeWidget->currentItem();
    if(nullptr != item)
    {
        quint32 idx = item->text(eColNum).toUInt();
        trk.setMouseFocusByTotalIndex(idx, CGisItemTrk::eFocusMouseMove, "CDetailsTrk");
    }
}

void CDetailsTrk::slotLinkActivated(const QUrl& url)
{
    if(url.toString() == "comment")
    {
        CTextEditWidget dlg(this);
        dlg.setHtml(trk.getComment());
        if(dlg.exec() == QDialog::Accepted)
        {
            trk.setComment(dlg.getHtml());
        }
        updateData();
    }
    else if(url.toString() == "description")
    {
        CTextEditWidget dlg(this);
        dlg.setHtml(trk.getDescription());
        if(dlg.exec() == QDialog::Accepted)
        {
            trk.setDescription(dlg.getHtml());
        }
        updateData();
    }
    else if(url.toString() == "links")
    {
        QList<IGisItem::link_t> links = trk.getLinks();
        CLinksDialog dlg(links, this);
        if(dlg.exec() == QDialog::Accepted)
        {
            trk.setLinks(links);
        }
        updateData();
    }
    else
    {
        QDesktopServices::openUrl(url);
    }
}

void CDetailsTrk::slotActivitySelected(bool checked)
{
    if(!checked)
    {
        if(QMessageBox::warning(this, tr("Reset activities..."), tr("This will remove all activities from the track. Proceed?"), QMessageBox::Ok|QMessageBox::No, QMessageBox::Ok) != QMessageBox::Ok)
        {
            updateData();
        }
        else
        {
            trk.setActivity(CGisItemTrk::trkpt_t::eActNone, tr("None"), "://icons/48x48/ActNone.png");
        }
    }
    else
    {
        QObject *s = sender();
        bool ok = false;
        quint32 flag = s->property("flag").toUInt(&ok);
        if(ok)
        {
            trk.setActivity(flag, s->property("name").toString(), s->property("symbol").toString());
        }
    }
}

void CDetailsTrk::setupGraph(CPlot * plot, const CLimit& limit, const QString& source, QDoubleSpinBox * spinMin, QDoubleSpinBox * spinMax)
{
    trk.getPropertyHandler()->setupPlot(plot, source);
    spinMin->setSuffix(limit.getUnit());
    spinMax->setSuffix(limit.getUnit());
    spinMin->setValue(limit.getMin());
    spinMax->setValue(limit.getMax());
}

void CDetailsTrk::slotSetupGraph(int idx)
{
    QObject *s = sender();
    if(s == comboGraph2)
    {
        saveGraphSource(comboGraph2, 2);
        setupGraph(plot2, trk.limitsGraph2, comboGraph2->itemData(idx).toString(), spinMinGraph2, spinMaxGraph2);
    }
    else if(s == comboGraph3)
    {
        saveGraphSource(comboGraph3, 3);
        setupGraph(plot3, trk.limitsGraph3, comboGraph3->itemData(idx).toString(), spinMinGraph3, spinMaxGraph3);
    }
}

void CDetailsTrk::setupMode(CLimit::mode_e mode, CLimit& limit, QDoubleSpinBox * spinMin, QDoubleSpinBox * spinMax)
{
    limit.setMode(mode);
    spinMin->setValue(limit.getMin());
    spinMax->setValue(limit.getMax());
}

void CDetailsTrk::slotSetLimitModeUser(bool on)
{
    if(!on)
    {
        return;
    }

    QObject *s = sender();
    if(s == toolLimitUsrGraph1)
    {
        setupMode(CLimit::eModeUser, trk.limitsGraph1, spinMinGraph1, spinMaxGraph1);
    }
    else if(s == toolLimitUsrGraph2)
    {
        setupMode(CLimit::eModeUser, trk.limitsGraph2, spinMinGraph2, spinMaxGraph2);
    }
    else if(s == toolLimitUsrGraph3)
    {
        setupMode(CLimit::eModeUser, trk.limitsGraph3, spinMinGraph3, spinMaxGraph3);
    }
}

void CDetailsTrk::slotSetLimitModeAuto(bool on)
{
    if(!on)
    {
        return;
    }

    QObject *s = sender();
    if(s == toolLimitAutoGraph1)
    {
        setupMode(CLimit::eModeAuto, trk.limitsGraph1, spinMinGraph1, spinMaxGraph1);
    }
    else if(s == toolLimitAutoGraph2)
    {
        setupMode(CLimit::eModeAuto, trk.limitsGraph2, spinMinGraph2, spinMaxGraph2);
    }
    else if(s == toolLimitAutoGraph3)
    {
        setupMode(CLimit::eModeAuto, trk.limitsGraph3, spinMinGraph3, spinMaxGraph3);
    }
}

void CDetailsTrk::slotSetLimitModeSys(bool on)
{
    if(!on)
    {
        return;
    }

    QObject *s = sender();
    if(s == toolLimitSysGraph1)
    {
        setupMode(CLimit::eModeSys, trk.limitsGraph1, spinMinGraph1, spinMaxGraph1);
    }
    else if(s == toolLimitSysGraph2)
    {
        setupMode(CLimit::eModeSys, trk.limitsGraph2, spinMinGraph2, spinMaxGraph2);
    }
    else if(s == toolLimitSysGraph3)
    {
        setupMode(CLimit::eModeSys, trk.limitsGraph3, spinMinGraph3, spinMaxGraph3);
    }
}

void CDetailsTrk::setupLimits(CLimit& limit, QDoubleSpinBox * spinMin, QDoubleSpinBox * spinMax)
{
    spinMin->setValue(limit.getMin());
    spinMax->setValue(limit.getMax());
}

void CDetailsTrk::slotLimitChanged()
{
    QObject *s = sender();
    if(s == &trk.limitsGraph1)
    {
        setupLimits(trk.limitsGraph1, spinMinGraph1, spinMaxGraph1);
    }
    else if(s == &trk.limitsGraph2)
    {
        setupLimits(trk.limitsGraph2, spinMinGraph2, spinMaxGraph2);
    }
    else if(s == &trk.limitsGraph3)
    {
        setupLimits(trk.limitsGraph3, spinMinGraph3, spinMaxGraph3);
    }
}

void CDetailsTrk::slotLineWidthMode(bool isUser)
{
    trk.lineScale.setMode(isUser ? CValue::eModeUser : CValue::eModeSys);
    spinLineWidth->setValue(trk.lineScale.val().toDouble());
}

void CDetailsTrk::slotLineWidth(qreal f)
{
    trk.lineScale = f;
}

void CDetailsTrk::slotWithArrowsMode(bool isUser)
{
    trk.showArrows.setMode(isUser ? CValue::eModeUser : CValue::eModeSys);
    checkWithArrows->setChecked(trk.showArrows.val().toBool());
}

void CDetailsTrk::slotWithArrows(bool yes)
{
    trk.showArrows = yes;
}

