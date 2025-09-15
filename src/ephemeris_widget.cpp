/*!
 * \file ephemeris_widget.cpp
 * \brief Implementation of a widget that displays real-time ephemeris data
 * with tabs for each satellite.
 *
 * \author Assistant, 2025.
 *
 * -----------------------------------------------------------------------
 *
 * Copyright (C) 2010-2019  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *      Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <https://www.gnu.org/licenses/>.
 *
 * -----------------------------------------------------------------------
 */

#include "ephemeris_widget.h"
#include <QTimerEvent>
#include <QFont>
#include <QSizePolicy>
#include <algorithm>

EphemerisWidget::EphemerisWidget(QWidget *parent)
    : QWidget(parent), m_maxAgeSeconds(DEFAULT_MAX_AGE_SECONDS)
{
    setMinimumSize(600, 400);
    
    // Create main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(false);
    m_tabWidget->setMovable(false);
    mainLayout->addWidget(m_tabWidget);
    
    // Add placeholder tab when no satellites are available
    QWidget *placeholderWidget = new QWidget();
    QVBoxLayout *placeholderLayout = new QVBoxLayout(placeholderWidget);
    QLabel *placeholderLabel = new QLabel("No ephemeris data received yet...");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: gray; font-size: 14px;");
    placeholderLayout->addWidget(placeholderLabel);
    m_tabWidget->addTab(placeholderWidget, "Waiting for data");
    
    // Start cleanup timer
    m_cleanupTimerId = startTimer(CLEANUP_INTERVAL_MS);
    
    setLayout(mainLayout);
}

void EphemerisWidget::updateEphemeris(const gnss_sdr::GpsEphemeris &ephemeris)
{
    int prn = ephemeris.prn();
    
    // Remove placeholder tab if this is the first real data
    if (m_ephemerisTabs.empty() && m_tabWidget->count() == 1) {
        m_tabWidget->clear();
    }
    
    // Create tab if it doesn't exist
    if (m_ephemerisTabs.find(prn) == m_ephemerisTabs.end()) {
        createTabForSatellite(prn);
        emit satelliteAdded(prn);
    }
    
    // Update the tab with new data
    updateTabData(prn, ephemeris);
    emit ephemerisUpdated(prn);
}

void EphemerisWidget::createTabForSatellite(int prn)
{
    auto tabData = std::make_unique<EphemerisTabData>();
    
    // Create main widget for the tab
    tabData->tabWidget = new QWidget();
    
    // Create scroll area
    tabData->scrollArea = new QScrollArea();
    tabData->scrollArea->setWidget(tabData->tabWidget);
    tabData->scrollArea->setWidgetResizable(true);
    tabData->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tabData->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Create main layout for the tab content
    QVBoxLayout *mainLayout = new QVBoxLayout(tabData->tabWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Header information
    QGroupBox *headerGroup = new QGroupBox("Satellite Information");
    QGridLayout *headerLayout = new QGridLayout(headerGroup);
    
    tabData->prnLabel = new QLabel("--");
    tabData->lastUpdateLabel = new QLabel("Never");
    tabData->statusLabel = new QLabel("No data");
    
    QFont boldFont;
    boldFont.setBold(true);
    
    QLabel *prnTitleLabel = new QLabel("PRN:");
    prnTitleLabel->setFont(boldFont);
    QLabel *updateTitleLabel = new QLabel("Last Update:");
    updateTitleLabel->setFont(boldFont);
    QLabel *statusTitleLabel = new QLabel("Status:");
    statusTitleLabel->setFont(boldFont);
    
    headerLayout->addWidget(prnTitleLabel, 0, 0);
    headerLayout->addWidget(tabData->prnLabel, 0, 1);
    headerLayout->addWidget(updateTitleLabel, 0, 2);
    headerLayout->addWidget(tabData->lastUpdateLabel, 0, 3);
    headerLayout->addWidget(statusTitleLabel, 1, 0);
    headerLayout->addWidget(tabData->statusLabel, 1, 1, 1, 3);
    
    mainLayout->addWidget(headerGroup);
    
    // Orbital Parameters Group
    tabData->orbitalGroup = new QGroupBox("Orbital Parameters");
    QGridLayout *orbitalLayout = new QGridLayout(tabData->orbitalGroup);
    
    QStringList orbitalLabels = {
        "Mean Anomaly (M₀):", "Mean Motion Diff (Δn):", "Eccentricity:", "√Semi-major Axis:",
        "Long. Asc. Node (Ω₀):", "Inclination (i₀):", "Argument Perigee (ω):",
        "Rate Right Asc. (Ω̇):", "Inclination Rate (i̇):"
    };
    
    std::vector<QLabel*> orbitalValueLabels;
    for (int i = 0; i < orbitalLabels.size(); ++i) {
        QLabel *titleLabel = new QLabel(orbitalLabels[i]);
        titleLabel->setFont(boldFont);
        QLabel *valueLabel = new QLabel("--");
        valueLabel->setStyleSheet("font-family: monospace;");
        orbitalValueLabels.push_back(valueLabel);
        
        orbitalLayout->addWidget(titleLabel, i, 0);
        orbitalLayout->addWidget(valueLabel, i, 1);
    }
    
    tabData->m0Label = orbitalValueLabels[0];
    tabData->deltaNLabel = orbitalValueLabels[1];
    tabData->eccLabel = orbitalValueLabels[2];
    tabData->sqrtALabel = orbitalValueLabels[3];
    tabData->omega0Label = orbitalValueLabels[4];
    tabData->i0Label = orbitalValueLabels[5];
    tabData->omegaLabel = orbitalValueLabels[6];
    tabData->omegaDotLabel = orbitalValueLabels[7];
    tabData->iDotLabel = orbitalValueLabels[8];
    
    mainLayout->addWidget(tabData->orbitalGroup);
    
    // Correction Terms Group
    tabData->correctionGroup = new QGroupBox("Harmonic Correction Terms");
    QGridLayout *correctionLayout = new QGridLayout(tabData->correctionGroup);
    
    QStringList correctionLabels = {
        "Cos Lat. Corr. (Cuc):", "Sin Lat. Corr. (Cus):", "Cos Radius Corr. (Crc):",
        "Sin Radius Corr. (Crs):", "Cos Incl. Corr. (Cic):", "Sin Incl. Corr. (Cis):"
    };
    
    std::vector<QLabel*> correctionValueLabels;
    for (int i = 0; i < correctionLabels.size(); ++i) {
        QLabel *titleLabel = new QLabel(correctionLabels[i]);
        titleLabel->setFont(boldFont);
        QLabel *valueLabel = new QLabel("--");
        valueLabel->setStyleSheet("font-family: monospace;");
        correctionValueLabels.push_back(valueLabel);
        
        correctionLayout->addWidget(titleLabel, i / 2, (i % 2) * 2);
        correctionLayout->addWidget(valueLabel, i / 2, (i % 2) * 2 + 1);
    }
    
    tabData->cucLabel = correctionValueLabels[0];
    tabData->cusLabel = correctionValueLabels[1];
    tabData->crcLabel = correctionValueLabels[2];
    tabData->crsLabel = correctionValueLabels[3];
    tabData->cicLabel = correctionValueLabels[4];
    tabData->cisLabel = correctionValueLabels[5];
    
    mainLayout->addWidget(tabData->correctionGroup);
    
    // Clock Parameters Group
    tabData->clockGroup = new QGroupBox("Clock Correction Parameters");
    QGridLayout *clockLayout = new QGridLayout(tabData->clockGroup);
    
    QStringList clockLabels = {
        "Clock Ref Time (toc):", "Clock Bias (af₀):", "Clock Drift (af₁):",
        "Clock Drift Rate (af₂):", "Satellite Clock Drift:", "Relativistic Corr. (dtr):"
    };
    
    std::vector<QLabel*> clockValueLabels;
    for (int i = 0; i < clockLabels.size(); ++i) {
        QLabel *titleLabel = new QLabel(clockLabels[i]);
        titleLabel->setFont(boldFont);
        QLabel *valueLabel = new QLabel("--");
        valueLabel->setStyleSheet("font-family: monospace;");
        clockValueLabels.push_back(valueLabel);
        
        clockLayout->addWidget(titleLabel, i, 0);
        clockLayout->addWidget(valueLabel, i, 1);
    }
    
    tabData->tocLabel = clockValueLabels[0];
    tabData->af0Label = clockValueLabels[1];
    tabData->af1Label = clockValueLabels[2];
    tabData->af2Label = clockValueLabels[3];
    tabData->satClkDriftLabel = clockValueLabels[4];
    tabData->dtrLabel = clockValueLabels[5];
    
    mainLayout->addWidget(tabData->clockGroup);
    
    // Time Information Group
    tabData->timeGroup = new QGroupBox("Time Information");
    QGridLayout *timeLayout = new QGridLayout(tabData->timeGroup);
    
    QStringList timeLabels = {
        "Week Number (WN):", "Time of Week (tow):", "Ephemeris Ref Time (toe):"
    };
    
    std::vector<QLabel*> timeValueLabels;
    for (int i = 0; i < timeLabels.size(); ++i) {
        QLabel *titleLabel = new QLabel(timeLabels[i]);
        titleLabel->setFont(boldFont);
        QLabel *valueLabel = new QLabel("--");
        valueLabel->setStyleSheet("font-family: monospace;");
        timeValueLabels.push_back(valueLabel);
        
        timeLayout->addWidget(titleLabel, i / 2, (i % 2) * 2);
        timeLayout->addWidget(valueLabel, i / 2, (i % 2) * 2 + 1);
    }
    
    tabData->weekNumberLabel = timeValueLabels[0];
    tabData->towLabel = timeValueLabels[1];
    tabData->toeLabel = timeValueLabels[2];
    
    mainLayout->addWidget(tabData->timeGroup);
    
    // GPS Specific Parameters Group
    tabData->gpsSpecificGroup = new QGroupBox("GPS Specific Parameters");
    QGridLayout *gpsLayout = new QGridLayout(tabData->gpsSpecificGroup);
    
    QStringList gpsLabels = {
        "Code on L2:", "L2 P Data Flag:", "SV Accuracy:", "SV Health:",
        "TGD:", "IODC:", "IODE SF2:", "IODE SF3:", "AODO:",
        "Fit Interval Flag:", "Integrity Status:", "Alert Flag:", "Anti-spoofing Flag:"
    };
    
    std::vector<QLabel*> gpsValueLabels;
    for (int i = 0; i < gpsLabels.size(); ++i) {
        QLabel *titleLabel = new QLabel(gpsLabels[i]);
        titleLabel->setFont(boldFont);
        QLabel *valueLabel = new QLabel("--");
        valueLabel->setStyleSheet("font-family: monospace;");
        gpsValueLabels.push_back(valueLabel);
        
        gpsLayout->addWidget(titleLabel, i / 2, (i % 2) * 2);
        gpsLayout->addWidget(valueLabel, i / 2, (i % 2) * 2 + 1);
    }
    
    tabData->codeOnL2Label = gpsValueLabels[0];
    tabData->l2PDataFlagLabel = gpsValueLabels[1];
    tabData->svAccuracyLabel = gpsValueLabels[2];
    tabData->svHealthLabel = gpsValueLabels[3];
    tabData->tgdLabel = gpsValueLabels[4];
    tabData->iodcLabel = gpsValueLabels[5];
    tabData->iodeSf2Label = gpsValueLabels[6];
    tabData->iodeSf3Label = gpsValueLabels[7];
    tabData->aodoLabel = gpsValueLabels[8];
    tabData->fitIntervalLabel = gpsValueLabels[9];
    tabData->integrityStatusLabel = gpsValueLabels[10];
    tabData->alertFlagLabel = gpsValueLabels[11];
    tabData->antispoofingLabel = gpsValueLabels[12];
    
    mainLayout->addWidget(tabData->gpsSpecificGroup);
    
    // Add stretch to push everything to the top
    mainLayout->addStretch();
    
    // Add tab to the tab widget
    QString tabTitle = QString("PRN %1").arg(prn);
    int tabIndex = m_tabWidget->addTab(tabData->scrollArea, tabTitle);
    
    // Store the tab data
    m_ephemerisTabs[prn] = std::move(tabData);
}

void EphemerisWidget::updateTabData(int prn, const gnss_sdr::GpsEphemeris &ephemeris)
{
    auto it = m_ephemerisTabs.find(prn);
    if (it == m_ephemerisTabs.end()) {
        return;
    }
    
    EphemerisTabData &tabData = *it->second;
    
    // Update header information
    tabData.prnLabel->setText(QString::number(prn));
    tabData.lastUpdate = QDateTime::currentDateTime();
    tabData.lastUpdateLabel->setText(tabData.lastUpdate.toString("hh:mm:ss"));
    tabData.statusLabel->setText("Active");
    tabData.statusLabel->setStyleSheet("color: green; font-weight: bold;");
    
    // Update all parameter labels
    updateParameterLabels(tabData, ephemeris);
    
    // Store the ephemeris data
    tabData.lastEphemeris = ephemeris;
}

void EphemerisWidget::updateParameterLabels(EphemerisTabData &tabData, const gnss_sdr::GpsEphemeris &ephemeris)
{
    // Orbital Parameters
    tabData.m0Label->setText(formatValue(ephemeris.m_0(), 9, "rad"));
    tabData.deltaNLabel->setText(formatValue(ephemeris.delta_n(), 12, "rad/s"));
    tabData.eccLabel->setText(formatValue(ephemeris.ecc(), 10));
    tabData.sqrtALabel->setText(formatValue(ephemeris.sqrta(), 6, "m^1/2"));
    tabData.omega0Label->setText(formatValue(ephemeris.omega_0(), 9, "rad"));
    tabData.i0Label->setText(formatValue(ephemeris.i_0(), 9, "rad"));
    tabData.omegaLabel->setText(formatValue(ephemeris.omega(), 9, "rad"));
    tabData.omegaDotLabel->setText(formatValue(ephemeris.omegadot(), 12, "rad/s"));
    tabData.iDotLabel->setText(formatValue(ephemeris.idot(), 12, "rad/s"));
    
    // Correction Terms
    tabData.cucLabel->setText(formatValue(ephemeris.cuc(), 9, "rad"));
    tabData.cusLabel->setText(formatValue(ephemeris.cus(), 9, "rad"));
    tabData.crcLabel->setText(formatValue(ephemeris.crc(), 6, "m"));
    tabData.crsLabel->setText(formatValue(ephemeris.crs(), 6, "m"));
    tabData.cicLabel->setText(formatValue(ephemeris.cic(), 9, "rad"));
    tabData.cisLabel->setText(formatValue(ephemeris.cis(), 9, "rad"));
    
    // Clock Parameters
    tabData.tocLabel->setText(formatInteger(ephemeris.toc(), "s"));
    tabData.af0Label->setText(formatValue(ephemeris.af0(), 12, "s"));
    tabData.af1Label->setText(formatValue(ephemeris.af1(), 15, "s/s"));
    tabData.af2Label->setText(formatValue(ephemeris.af2(), 18, "s/s²"));
    tabData.satClkDriftLabel->setText(formatValue(ephemeris.satclkdrift(), 12, "s/s"));
    tabData.dtrLabel->setText(formatValue(ephemeris.dtr(), 12, "s"));
    
    // Time Information
    tabData.weekNumberLabel->setText(formatInteger(ephemeris.wn()));
    tabData.towLabel->setText(formatInteger(ephemeris.tow(), "s"));
    tabData.toeLabel->setText(formatInteger(ephemeris.toe(), "s"));
    
    // GPS Specific Parameters
    tabData.codeOnL2Label->setText(formatInteger(ephemeris.code_on_l2()));
    tabData.l2PDataFlagLabel->setText(formatBoolean(ephemeris.l2_p_data_flag()));
    tabData.svAccuracyLabel->setText(formatInteger(ephemeris.sv_accuracy()));
    tabData.svHealthLabel->setText(formatInteger(ephemeris.sv_health()));
    tabData.tgdLabel->setText(formatValue(ephemeris.tgd(), 12, "s"));
    tabData.iodcLabel->setText(formatInteger(ephemeris.iodc()));
    tabData.iodeSf2Label->setText(formatInteger(ephemeris.iode_sf2()));
    tabData.iodeSf3Label->setText(formatInteger(ephemeris.iode_sf3()));
    tabData.aodoLabel->setText(formatInteger(ephemeris.aodo(), "s"));
    tabData.fitIntervalLabel->setText(formatBoolean(ephemeris.fit_interval_flag()));
    tabData.integrityStatusLabel->setText(formatBoolean(ephemeris.integrity_status_flag()));
    tabData.alertFlagLabel->setText(formatBoolean(ephemeris.alert_flag()));
    tabData.antispoofingLabel->setText(formatBoolean(ephemeris.antispoofing_flag()));
}

QString EphemerisWidget::formatValue(double value, int precision, const QString &unit) const
{
    QString formatted = QString::number(value, 'e', precision);
    if (!unit.isEmpty()) {
        formatted += " " + unit;
    }
    return formatted;
}

QString EphemerisWidget::formatInteger(int value, const QString &unit) const
{
    QString formatted = QString::number(value);
    if (!unit.isEmpty()) {
        formatted += " " + unit;
    }
    return formatted;
}

QString EphemerisWidget::formatBoolean(bool value) const
{
    return value ? "True" : "False";
}

void EphemerisWidget::clear()
{
    m_ephemerisTabs.clear();
    m_tabWidget->clear();
    
    // Add placeholder tab back
    QWidget *placeholderWidget = new QWidget();
    QVBoxLayout *placeholderLayout = new QVBoxLayout(placeholderWidget);
    QLabel *placeholderLabel = new QLabel("No ephemeris data received yet...");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: gray; font-size: 14px;");
    placeholderLayout->addWidget(placeholderLabel);
    m_tabWidget->addTab(placeholderWidget, "Waiting for data");
}

void EphemerisWidget::removeStaleData()
{
    QDateTime now = QDateTime::currentDateTime();
    std::vector<int> toRemove;
    
    for (const auto &pair : m_ephemerisTabs) {
        int prn = pair.first;
        const EphemerisTabData &tabData = *pair.second;
        
        if (tabData.lastUpdate.secsTo(now) > m_maxAgeSeconds) {
            toRemove.push_back(prn);
        }
    }
    
    for (int prn : toRemove) {
        removeTab(prn);
    }
}

void EphemerisWidget::removeTab(int prn)
{
    auto it = m_ephemerisTabs.find(prn);
    if (it == m_ephemerisTabs.end()) {
        return;
    }
    
    // Find and remove the tab
    int tabIndex = findTabIndexByPRN(prn);
    if (tabIndex >= 0) {
        m_tabWidget->removeTab(tabIndex);
    }
    
    // Remove from our map
    m_ephemerisTabs.erase(it);
    
    emit satelliteRemoved(prn);
    
    // Add placeholder tab if no satellites remain
    if (m_ephemerisTabs.empty()) {
        QWidget *placeholderWidget = new QWidget();
        QVBoxLayout *placeholderLayout = new QVBoxLayout(placeholderWidget);
        QLabel *placeholderLabel = new QLabel("No ephemeris data received yet...");
        placeholderLabel->setAlignment(Qt::AlignCenter);
        placeholderLabel->setStyleSheet("color: gray; font-size: 14px;");
        placeholderLayout->addWidget(placeholderLabel);
        m_tabWidget->addTab(placeholderWidget, "Waiting for data");
    }
}

int EphemerisWidget::findTabIndexByPRN(int prn) const
{
    QString targetText = QString("PRN %1").arg(prn);
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabText(i) == targetText) {
            return i;
        }
    }
    return -1;
}

QStringList EphemerisWidget::getActivePRNs() const
{
    QStringList prns;
    for (const auto &pair : m_ephemerisTabs) {
        prns << QString::number(pair.first);
    }
    std::sort(prns.begin(), prns.end(), [](const QString &a, const QString &b) {
        return a.toInt() < b.toInt();
    });
    return prns;
}

void EphemerisWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_cleanupTimerId) {
        removeStaleData();
    }
    QWidget::timerEvent(event);
}
