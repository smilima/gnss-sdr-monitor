/*!
 * \file ephemeris_widget.h
 * \brief Interface of a widget that displays real-time ephemeris data
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

#ifndef GNSS_SDR_MONITOR_EPHEMERIS_WIDGET_H_
#define GNSS_SDR_MONITOR_EPHEMERIS_WIDGET_H_

#include "gps_ephemeris.pb.h"
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QGridLayout>
#include <QDateTime>
#include <map>
#include <memory>

struct EphemerisTabData
{
    QWidget* tabWidget;
    QScrollArea* scrollArea;
    QLabel* lastUpdateLabel;
    QLabel* prnLabel;
    QLabel* statusLabel;
    
    // Orbital Parameters Group
    QGroupBox* orbitalGroup;
    QLabel* m0Label;
    QLabel* deltaNLabel;
    QLabel* eccLabel;
    QLabel* sqrtALabel;
    QLabel* omega0Label;
    QLabel* i0Label;
    QLabel* omegaLabel;
    QLabel* omegaDotLabel;
    QLabel* iDotLabel;
    
    // Correction Terms Group
    QGroupBox* correctionGroup;
    QLabel* cucLabel;
    QLabel* cusLabel;
    QLabel* crcLabel;
    QLabel* crsLabel;
    QLabel* cicLabel;
    QLabel* cisLabel;
    
    // Clock Parameters Group
    QGroupBox* clockGroup;
    QLabel* tocLabel;
    QLabel* af0Label;
    QLabel* af1Label;
    QLabel* af2Label;
    QLabel* satClkDriftLabel;
    QLabel* dtrLabel;
    
    // Time Information Group
    QGroupBox* timeGroup;
    QLabel* weekNumberLabel;
    QLabel* towLabel;
    QLabel* toeLabel;
    
    // GPS Specific Group
    QGroupBox* gpsSpecificGroup;
    QLabel* codeOnL2Label;
    QLabel* l2PDataFlagLabel;
    QLabel* svAccuracyLabel;
    QLabel* svHealthLabel;
    QLabel* tgdLabel;
    QLabel* iodcLabel;
    QLabel* iodeSf2Label;
    QLabel* iodeSf3Label;
    QLabel* aodoLabel;
    QLabel* fitIntervalLabel;
    QLabel* integrityStatusLabel;
    QLabel* alertFlagLabel;
    QLabel* antispoofingLabel;
    
    QDateTime lastUpdate;
    gnss_sdr::GpsEphemeris lastEphemeris;
    
    EphemerisTabData() : tabWidget(nullptr), scrollArea(nullptr), lastUpdateLabel(nullptr),
                        prnLabel(nullptr), statusLabel(nullptr), orbitalGroup(nullptr),
                        correctionGroup(nullptr), clockGroup(nullptr), timeGroup(nullptr),
                        gpsSpecificGroup(nullptr) {}
};

class EphemerisWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EphemerisWidget(QWidget *parent = nullptr);
    ~EphemerisWidget() override = default;

    // Configuration
    void setMaxAge(int seconds) { m_maxAgeSeconds = seconds; }
    int getMaxAge() const { return m_maxAgeSeconds; }
    
    // Statistics
    int getSatelliteCount() const { return m_ephemerisTabs.size(); }
    QStringList getActivePRNs() const;

public slots:
    void updateEphemeris(const gnss_sdr::GpsEphemeris &ephemeris);
    void clear();
    void removeStaleData();

signals:
    void ephemerisUpdated(int prn);
    void satelliteAdded(int prn);
    void satelliteRemoved(int prn);

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void createTabForSatellite(int prn);
    void updateTabData(int prn, const gnss_sdr::GpsEphemeris &ephemeris);
    void removeTab(int prn);
    QWidget* createParameterGroup(const QString &title, const QStringList &labels, 
                                 std::vector<QLabel*> &valueLabels);
    void updateParameterLabels(EphemerisTabData &tabData, const gnss_sdr::GpsEphemeris &ephemeris);
    QString formatValue(double value, int precision = 6, const QString &unit = "") const;
    QString formatInteger(int value, const QString &unit = "") const;
    QString formatBoolean(bool value) const;
    int findTabIndexByPRN(int prn) const;
    
    QTabWidget* m_tabWidget;
    std::map<int, std::unique_ptr<EphemerisTabData>> m_ephemerisTabs;  // PRN -> tab data
    
    int m_maxAgeSeconds;
    int m_cleanupTimerId;
    
    static constexpr int CLEANUP_INTERVAL_MS = 5000;  // 5 seconds
    static constexpr int DEFAULT_MAX_AGE_SECONDS = 300;  // 5 minutes
};

#endif  // GNSS_SDR_MONITOR_EPHEMERIS_WIDGET_H_