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

#include "CMainWindow.h"
#include "gis/trk/CGisItemTrk.h"
#include "gis/wpt/CGisItemWpt.h"
#include "plot/CPlotAxis.h"
#include "plot/CPlotProfile.h"
#include "units/IUnit.h"
#include <interpolation.h>

#include <proj_api.h>

CPlotProfile::CPlotProfile(QWidget * parent)
    : IPlot(nullptr, CPlotData::eAxisLinear, eModeNormal, parent)
{
}

CPlotProfile::CPlotProfile(CGisItemTrk *trk, CLimit& lim, mode_e mode, QWidget *parent)
    : IPlot(trk, CPlotData::eAxisLinear, mode, parent)
    , limit(&lim)
{
    connect(limit, &CLimit::sigChanged, this, &CPlotProfile::setLimits);
    setWindowTitle(trk->getNameEx());
    updateData();
}

CPlotProfile::~CPlotProfile()
{
}

void CPlotProfile::setTrack(CGisItemTrk * track, CLimit &lim)
{
    trk = track;
    trk->registerVisual(this);

    if(limit)
    {
        disconnect(limit, &CLimit::sigChanged, this, &CPlotProfile::setLimits);
    }
    limit = &lim;
    connect(limit, &CLimit::sigChanged, this, &CPlotProfile::setLimits);

    updateData();
}

#define Q 32
void CPlotProfile::updateData()
{
    clear();

    qreal scale;
    QString unit;
    IUnit::self().meter2unit(trk->getTotalDistance(), scale, unit);
    setXTicScale(scale);

    if(mode == eModeIcon)
    {
        setXLabel(trk->getName());
        setYLabel("");
    }
    else
    {
        setXLabel(tr("distance [%1]").arg(unit));
        setYLabel(tr("alt. [%1]").arg(IUnit::self().baseunit));
    }

    QPolygonF lineEle;
    QPolygonF lineDem;
    QPolygonF coords;

    const qint32 N = trk->getNumberOfVisiblePoints();
    alglib::real_1d_array X,Y;
    X.setlength(N);
    Y.setlength(N);

    IGisProject * project = dynamic_cast<IGisProject*>(trk->parent());

    qreal basefactor = IUnit::self().basefactor;
    const CGisItemTrk::trk_t& t = trk->getTrackData();
    for(const CGisItemTrk::trkseg_t& seg : t.segs)
    {
        for(const CGisItemTrk::trkpt_t& trkpt : seg.pts)
        {
            if(trkpt.flags & CGisItemTrk::trkpt_t::eHidden)
            {
                continue;
            }

            if(trkpt.ele == NOINT)
            {
                continue;
            }

            X[trkpt.idxVisible] = trkpt.distance;
            Y[trkpt.idxVisible] = trkpt.ele * basefactor;

            lineEle << QPointF(trkpt.distance, trkpt.ele * basefactor);
            coords << QPointF(trkpt.lon * DEG_TO_RAD, trkpt.lat * DEG_TO_RAD);
            lineDem << QPointF(trkpt.distance, NOFLOAT);

            if(nullptr == project || trkpt.keyWpt.item.isEmpty() || (mode == eModeIcon))
            {
                continue;
            }

            CGisItemWpt * wpt = dynamic_cast<CGisItemWpt*>(project->getItemByKey(trkpt.keyWpt));
            if(wpt)
            {
                CPlotData::point_t tag;
                tag.point = lineEle.last();
                tag.icon  = wpt->getIcon();
                tag.label = wpt->getName();
                data->tags << tag;
            }
        }
    }

    CMainWindow::self().getElevationAt(coords, lineDem);

    alglib::ae_int_t info;
    alglib::ae_int_t m = N/Q;
    alglib::spline1dinterpolant p;
    alglib::spline1dfitreport rep;

    alglib::spline1dfitcubic(X,Y,m, info, p, rep);

    QPolygonF spline;
    const qreal maxDist = trk->getTotalDistance();
    const qreal deltaDist = maxDist / (N);
    qreal dist = 0;
    while(dist < maxDist)
    {
        spline << QPointF(dist, alglib::spline1dcalc(p, dist));
        dist += deltaDist;
    }

    newLine(lineEle, "GPS");
    if(!lineDem.isEmpty())
    {
        addLine(lineDem, "DEM");
    }
    addLine(spline,"spline");
    setLimits();
    resetZoom();
}

void CPlotProfile::setMouseFocus(const CGisItemTrk::trkpt_t * ptMouseMove)
{
    if(nullptr == ptMouseMove)
    {
        if(posMouse1 != NOPOINT)
        {
            posMouse1 = NOPOINT;
            needsRedraw = true;
        }
    }
    else
    {
        if(posMouse1 == NOPOINT)
        {
            needsRedraw = true;
        }

        posMouse1.rx() = left + data->x().val2pt(ptMouseMove->distance);
        posMouse1.ry() = top  + data->y().val2pt(ptMouseMove->ele);
    }
    update();
}

void CPlotProfile::setLimits()
{
    IPlot::setLimits();
    data->ymin = limit->getMin() == NOFLOAT ? data->ymin : limit->getMin();
    data->ymax = limit->getMax() == NOFLOAT ? data->ymax : limit->getMax();

    data->y().setLimits(data->ymin, data->ymax);
    resetZoom();
    update();
}
