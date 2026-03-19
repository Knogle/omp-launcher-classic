/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * This file contains modifications and derivative sections based on
 * https://github.com/openmultiplayer/launcher .
 *
 * In particular, the server list sourcing, partner-tab mapping, open.mp
 * client path handling, and Windows launch-helper argument flow are
 * informed by the upstream launcher implementation, especially:
 * - src/utils/game.ts
 * - src-tauri/src/main.rs
 * - src-tauri/src/constants.rs
 *
 * The surrounding Qt UI and non-listed sections are project-specific.
 */

#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QPixmap>
#include <QtGui/QShowEvent>
#include <QtGui/QWindow>
#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QQueue>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QUdpSocket>
#include <QtCore/QProcess>
#include <QtSvg/QSvgRenderer>
#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
extern "C" HRESULT WINAPI SetCurrentProcessExplicitAppUserModelID(PCWSTR app_id);
#endif

constexpr auto kServerListUrl = "https://api.open.mp/servers/full";
constexpr auto kOmpClientDownloadUrl = "https://assets.open.mp/omp-client.dll";
constexpr auto kHelperPasswordEnvVar = "OMP_LAUNCHER_SERVER_PASSWORD";
constexpr int kDefaultPort = 7777;
constexpr int kPingTimeoutValue = 9999;
constexpr int kQueryTimeoutMs = 1500;
constexpr int kMaxPingSamples = 24;
constexpr int kMaxConcurrentPingJobs = 12;
constexpr char kSampHeader[] = {'S', 'A', 'M', 'P'};

constexpr int kServerKeyRole = Qt::UserRole;
constexpr int kSortValueRole = Qt::UserRole + 1;

// MPL-covered integration point derived from the upstream launcher:
// these constants and the related refresh/launch logic below mirror the
// open.mp launcher data source and client download expectations.
constexpr auto kLockedSvg = R"svg(<?xml version="1.0" encoding="utf-8"?>
<svg viewBox="0.474 0 12.052 16" fill="none" xmlns="http://www.w3.org/2000/svg">
  <path d="M1.625 16C1.31563 16 1.05078 15.8881 0.830469 15.6643C0.610156 15.4405 0.5 15.1714 0.5 14.8571V6.59048C0.5 6.27619 0.610156 6.00714 0.830469 5.78333C1.05078 5.55952 1.31563 5.44762 1.625 5.44762H2.9375V3.61905C2.9375 2.61778 3.28504 1.76429 3.98011 1.05857C4.67519 0.352857 5.51581 0 6.50199 0C7.48816 0 8.32813 0.352857 9.02188 1.05857C9.71563 1.76429 10.0625 2.61778 10.0625 3.61905V5.44762H11.375C11.6844 5.44762 11.9492 5.55952 12.1695 5.78333C12.3898 6.00714 12.5 6.27619 12.5 6.59048V14.8571C12.5 15.1714 12.3898 15.4405 12.1695 15.6643C11.9492 15.8881 11.6844 16 11.375 16H1.625ZM1.625 14.8571H11.375V6.59048H1.625V14.8571ZM6.50315 12.1905C6.90105 12.1905 7.24063 12.0506 7.52188 11.7708C7.80313 11.4911 7.94375 11.1548 7.94375 10.7619C7.94375 10.381 7.80207 10.0349 7.51872 9.72381C7.23537 9.4127 6.89475 9.25714 6.49685 9.25714C6.09895 9.25714 5.75938 9.4127 5.47813 9.72381C5.19688 10.0349 5.05625 10.3841 5.05625 10.7714C5.05625 11.1587 5.19793 11.4921 5.48128 11.7714C5.76463 12.0508 6.10525 12.1905 6.50315 12.1905ZM4.0625 5.44762H8.9375V3.61905C8.9375 2.93121 8.70074 2.34656 8.22721 1.86509C7.75367 1.3836 7.17868 1.14286 6.50221 1.14286C5.82574 1.14286 5.25 1.3836 4.775 1.86509C4.3 2.34656 4.0625 2.93121 4.0625 3.61905V5.44762Z" style="fill: rgb(0, 0, 0);" transform="matrix(1, 0, 0, 1, 0, -2.220446049250313e-16)"/>
</svg>
)svg";

constexpr auto kUnlockedSvg = R"svg(<?xml version="1.0" encoding="utf-8"?>
<svg viewBox="0 0 12 15.763" fill="none" xmlns="http://www.w3.org/2000/svg">
  <path d="M 1.125 5.378 L 8.438 5.378 L 8.438 3.578 C 8.438 2.901 8.201 2.326 7.727 1.852 C 7.254 1.378 6.679 1.141 6.002 1.141 C 5.326 1.141 4.75 1.378 4.275 1.852 C 3.8 2.326 3.563 2.901 3.563 3.578 L 2.438 3.578 C 2.438 2.591 2.785 1.75 3.48 1.056 C 4.175 0.363 5.016 0.016 6.002 0.016 C 6.988 0.016 7.828 0.363 8.522 1.058 C 9.216 1.752 9.563 2.593 9.563 3.578 L 9.563 5.378 L 10.875 5.378 C 11.184 5.378 11.449 5.488 11.67 5.709 C 11.89 5.929 12 6.194 12 6.503 L 12 14.641 C 12 14.95 11.89 15.215 11.67 15.435 C 11.449 15.655 11.184 15.766 10.875 15.766 L 1.125 15.766 C 0.816 15.766 0.551 15.655 0.33 15.435 C 0.11 15.215 0 14.95 0 14.641 L 0 6.503 C 0 6.194 0.11 5.929 0.33 5.709 C 0.551 5.488 0.816 5.378 1.125 5.378 Z M 1.125 14.641 L 10.875 14.641 L 10.875 6.503 L 1.125 6.503 L 1.125 14.641 Z M 6.003 12.016 C 6.401 12.016 6.741 11.878 7.022 11.603 C 7.303 11.327 7.444 10.996 7.444 10.609 C 7.444 10.234 7.302 9.894 7.019 9.588 C 6.735 9.281 6.395 9.128 5.997 9.128 C 5.599 9.128 5.259 9.281 4.978 9.588 C 4.697 9.894 4.556 10.238 4.556 10.619 C 4.556 11 4.698 11.328 4.981 11.603 C 5.265 11.878 5.605 12.016 6.003 12.016 Z" style="fill: rgb(0, 0, 0);" transform="matrix(1, 0, 0, 1, 0, -2.220446049250313e-16)"/>
</svg>
)svg";

struct PlayerEntry {
  QString name;
  int score = 0;
};

struct FavoriteRecord {
  QString name;
  QString alias;
  QString host;
  quint16 port = kDefaultPort;
  QString serverPassword;
};

struct LauncherSettings {
  bool autoSaveServerPasswords = false;
  QString gamePath;
  QString ompLauncherPath;
};

struct ServerData {
  bool favorite = false;
  bool locked = false;
  bool usingOmp = false;
  bool partner = false;
  QString hostname;
  QString host;
  quint16 port = kDefaultPort;
  int players = 0;
  int maxPlayers = 0;
  int ping = -1;
  QString mode;
  QString language;
  QString version;
  QVector<PlayerEntry> playerEntries;
  QVector<QPair<QString, QString>> rules;
  QVector<int> pingSamples;
  QDateTime lastQueryAt;
};

class SortableTableItem final : public QTableWidgetItem {
 public:
  SortableTableItem(const QString &text, const QVariant &sortValue)
      : QTableWidgetItem(text) {
    setData(kSortValueRole, sortValue);
  }

  bool operator<(const QTableWidgetItem &other) const override {
    const auto left = data(kSortValueRole);
    const auto right = other.data(kSortValueRole);
    if (left.isValid() && right.isValid()) {
      if (left.userType() == QMetaType::QString || right.userType() == QMetaType::QString) {
        return left.toString().localeAwareCompare(right.toString()) < 0;
      }
      return left.toLongLong() < right.toLongLong();
    }
    return QTableWidgetItem::operator<(other);
  }
};

QPixmap makeSvgPixmap(const QByteArray &svgData, const QSize &size, const QColor &color) {
  QSvgRenderer renderer(svgData);
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(pixmap.rect(), color);
  return pixmap;
}

QIcon makeStatusIcon(const QByteArray &svgData, const QColor &accentColor) {
  const QSize glyphSize(12, 16);
  const QSize canvasSize(16, 20);
  const QPoint origin((canvasSize.width() - glyphSize.width()) / 2,
                      (canvasSize.height() - glyphSize.height()) / 2);

  const QPixmap glyph = makeSvgPixmap(svgData, glyphSize, accentColor);
  const QPixmap shadow = makeSvgPixmap(svgData, glyphSize, QColor(0, 0, 0, 120));
  const QPixmap outline = makeSvgPixmap(svgData, glyphSize, QColor(255, 255, 255, 90));

  QPixmap canvas(canvasSize);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  for (const QPoint &offset : {QPoint(-1, 0), QPoint(1, 0), QPoint(0, -1), QPoint(0, 1)}) {
    painter.drawPixmap(origin + offset, outline);
  }
  painter.drawPixmap(origin + QPoint(0, 1), shadow);
  painter.drawPixmap(origin, glyph);
  return QIcon(canvas);
}

QIcon makeMonochromeSvgIcon(const QByteArray &svgData, const QSize &glyphSize, const QSize &canvasSize,
                            const QColor &color) {
  const QPoint origin((canvasSize.width() - glyphSize.width()) / 2,
                      (canvasSize.height() - glyphSize.height()) / 2);
  const QPixmap glyph = makeSvgPixmap(svgData, glyphSize, color);

  QPixmap canvas(canvasSize);
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.drawPixmap(origin, glyph);
  return QIcon(canvas);
}

const QIcon &launcherAppIcon() {
  static const QIcon icon = []() {
    QIcon resolvedIcon;
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.ico"));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(256, 256));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(128, 128));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(64, 64));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(48, 48));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(32, 32));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(24, 24));
    resolvedIcon.addFile(QStringLiteral(":/assets/samp_icon.png"), QSize(16, 16));
    return resolvedIcon;
  }();
  return icon;
}

#ifdef Q_OS_WIN
void applyNativeWindowsIcon(QWidget *widget) {
  if (!widget) {
    return;
  }

  const auto hwnd = reinterpret_cast<HWND>(widget->winId());
  if (!hwnd) {
    return;
  }

  const int bigWidth = GetSystemMetrics(SM_CXICON);
  const int bigHeight = GetSystemMetrics(SM_CYICON);
  const int smallWidth = GetSystemMetrics(SM_CXSMICON);
  const int smallHeight = GetSystemMetrics(SM_CYSMICON);

  auto *bigIcon = static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), L"IDI_APP_ICON", IMAGE_ICON, bigWidth, bigHeight, LR_DEFAULTCOLOR));
  auto *smallIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), L"IDI_APP_ICON", IMAGE_ICON, smallWidth,
                                                  smallHeight, LR_DEFAULTCOLOR));

  if (bigIcon) {
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
  }
  if (smallIcon) {
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
  }
}
#endif

void applyRuntimeWindowIcon(QWidget *widget) {
  if (!widget) {
    return;
  }

  widget->setWindowIcon(launcherAppIcon());
  if (auto *window = widget->windowHandle()) {
    window->setIcon(launcherAppIcon());
  }

#ifdef Q_OS_WIN
  applyNativeWindowsIcon(widget);
#endif
}

QColor actionIconColor(const QWidget *widget) {
  QColor color = widget->palette().color(QPalette::ButtonText);
  if (!color.isValid()) {
    color = QColor(230, 230, 230);
  }
  return color;
}

QIcon makePlayActionIcon(const QColor &color) {
  QPixmap canvas(QSize(18, 18));
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawPolygon(QPolygonF{{5.2, 3.8}, {5.2, 14.2}, {13.6, 9.0}});
  return QIcon(canvas);
}

QIcon makeCloseActionIcon(const QColor &color) {
  QPixmap canvas(QSize(18, 18));
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawLine(QPointF(5.0, 5.0), QPointF(13.0, 13.0));
  painter.drawLine(QPointF(13.0, 5.0), QPointF(5.0, 13.0));
  return QIcon(canvas);
}

QIcon makeRefreshActionIcon(const QColor &color) {
  QPixmap canvas(QSize(18, 18));
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  painter.drawArc(QRectF(3.6, 3.6, 10.8, 10.8), 30 * 16, 265 * 16);
  painter.drawLine(QPointF(12.7, 4.6), QPointF(14.2, 4.9));
  painter.drawLine(QPointF(12.7, 4.6), QPointF(13.7, 6.0));
  return QIcon(canvas);
}

QIcon makeInfoActionIcon(const QColor &color) {
  QPixmap canvas(QSize(18, 18));
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(QRectF(3.5, 3.5, 11.0, 11.0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawEllipse(QPointF(9.0, 5.7), 1.0, 1.0);
  painter.drawRoundedRect(QRectF(8.1, 7.5, 1.8, 5.0), 0.7, 0.7);
  return QIcon(canvas);
}

QIcon makeFavoriteIcon() {
  const QSize canvasSize(18, 18);
  QPixmap canvas(canvasSize);
  canvas.fill(Qt::transparent);

  const QVector<QPointF> points = {
      {9.0, 1.3},  {10.9, 5.8}, {15.5, 6.2}, {12.0, 9.4}, {13.2, 14.1},
      {9.0, 11.6}, {4.8, 14.1}, {6.0, 9.4},  {2.5, 6.2},  {7.1, 5.8},
  };

  auto makeStarPath = [&](const QPointF &offset) {
    QPainterPath path;
    path.moveTo(points[0] + offset);
    for (int i = 1; i < points.size(); ++i) {
      path.lineTo(points[i] + offset);
    }
    path.closeSubpath();
    return path;
  };

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0, 0, 0, 90));
  painter.drawPath(makeStarPath(QPointF(0.0, 0.8)));

  painter.setPen(QPen(QColor(255, 244, 196, 140), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(QColor(255, 205, 64));
  painter.drawPath(makeStarPath(QPointF(0.0, 0.0)));
  return QIcon(canvas);
}

QIcon makeServerPropertiesIcon(const QColor &color) {
  QPixmap canvas(QSize(18, 18));
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  painter.drawRoundedRect(QRectF(3.6, 2.8, 7.2, 10.2), 1.0, 1.0);
  painter.drawLine(QPointF(5.1, 6.0), QPointF(9.2, 6.0));
  painter.drawLine(QPointF(5.1, 8.2), QPointF(9.2, 8.2));

  painter.save();
  painter.translate(11.9, 11.8);
  painter.rotate(-35.0);
  painter.drawRoundedRect(QRectF(-0.9, -4.2, 1.8, 6.8), 0.5, 0.5);
  painter.drawLine(QPointF(0.0, -5.4), QPointF(0.0, -4.2));
  painter.restore();
  return QIcon(canvas);
}

QIcon makeSettingsIcon(const QColor &color) {
  QPixmap canvas(QSize(18, 18));
  canvas.fill(Qt::transparent);

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(9.0, 9.0);
  painter.setPen(QPen(color, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  for (int i = 0; i < 8; ++i) {
    painter.save();
    painter.rotate(i * 45.0);
    painter.drawLine(QPointF(0.0, -6.4), QPointF(0.0, -4.5));
    painter.restore();
  }

  painter.drawEllipse(QRectF(-4.2, -4.2, 8.4, 8.4));
  painter.drawEllipse(QRectF(-1.9, -1.9, 3.8, 3.8));
  return QIcon(canvas);
}

const QIcon &favoriteStateIcon() {
  static const QIcon favoriteIcon = makeFavoriteIcon();
  return favoriteIcon;
}

const QIcon &headerLockIcon() {
  static const QIcon icon =
      makeMonochromeSvgIcon(QByteArray(kLockedSvg), QSize(10, 13), QSize(14, 16), QColor(245, 245, 245));
  return icon;
}

const QIcon &passwordStateIcon(bool locked) {
  static const QIcon lockedIcon = makeStatusIcon(QByteArray(kLockedSvg), QColor(255, 92, 92));
  static const QIcon unlockedIcon = makeStatusIcon(QByteArray(kUnlockedSvg), QColor(54, 214, 106));
  return locked ? lockedIcon : unlockedIcon;
}

struct QueryOptions {
  bool info = false;
  bool players = false;
  bool rules = false;
  bool extraInfo = false;
  bool ping = false;
};

struct QueryResult {
  bool success = false;
  bool infoReceived = false;
  bool playersReceived = false;
  bool rulesReceived = false;
  bool pingReceived = false;
  bool extraInfoReceived = false;
  bool locked = false;
  int players = 0;
  int maxPlayers = 0;
  int ping = -1;
  QString hostname;
  QString gamemode;
  QString language;
  QVector<PlayerEntry> playerEntries;
  QVector<QPair<QString, QString>> rules;
  QString error;
};

QString serverKey(const QString &host, quint16 port) {
  return QString("%1:%2").arg(host).arg(port);
}

QString displayPing(int ping) {
  if (ping < 0) {
    return "-";
  }
  if (ping >= kPingTimeoutValue) {
    return "*";
  }
  return QString::number(ping);
}

QString unresolvedFavoriteHostname(const QString &host, quint16 port) {
  return QString("(Retrieving info...) %1").arg(serverKey(host, port));
}

bool isUnresolvedFavoriteHostname(const QString &hostname, const QString &host, quint16 port) {
  return hostname == unresolvedFavoriteHostname(host, port);
}

QString decodeSampString(const QByteArray &data) {
  QString value = QString::fromUtf8(data);
  if (value.contains(QChar::ReplacementCharacter)) {
    value = QString::fromLatin1(data);
  }
  return value.trimmed();
}

QByteArray buildQueryPacket(const QHostAddress &address, quint16 port, char type) {
  QByteArray packet;
  packet.append(kSampHeader, 4);
  const auto octets = address.toString().split('.');
  for (int i = 0; i < 4; ++i) {
    packet.append(char(octets.value(i).toInt()));
  }
  packet.append(char(port & 0xff));
  packet.append(char((port >> 8) & 0xff));
  packet.append(type);
  if (type == 'p') {
    packet.append(char(0));
    packet.append(char(0));
    packet.append(char(0));
    packet.append(char(0));
  }
  return packet;
}

bool parseEndpoint(const QString &endpoint, QString *host, quint16 *port) {
  const int separator = endpoint.lastIndexOf(':');
  if (separator <= 0) {
    return false;
  }

  bool ok = false;
  const auto parsedPort = endpoint.mid(separator + 1).toUShort(&ok);
  if (!ok) {
    return false;
  }

  *host = endpoint.left(separator);
  *port = parsedPort;
  return true;
}

bool isUsingOmp(const QVector<QPair<QString, QString>> &rules, const QString &version) {
  for (const auto &rule : rules) {
    if (rule.first == "allowed_clients") {
      return true;
    }
    if (rule.first == "version" && rule.second.contains("omp", Qt::CaseInsensitive)) {
      return true;
    }
  }
  return version.contains("omp", Qt::CaseInsensitive);
}

#ifdef Q_OS_WIN
constexpr int kInjectionMaxRetries = 5;
constexpr int kInjectionRetryDelayMs = 500;
constexpr int kProcessModuleBufferSize = 1024;

QString formatWindowsErrorMessage(DWORD errorCode) {
  if (errorCode == 0) {
    return "Unknown Windows error";
  }

  wchar_t *buffer = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD size = FormatMessageW(flags, nullptr, errorCode, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  QString message;
  if (size != 0 && buffer != nullptr) {
    message = QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed();
    LocalFree(buffer);
  } else {
    message = QString("Windows error %1").arg(errorCode);
  }
  return message;
}

bool processHasModule(DWORD processId, const QString &moduleSubstring) {
  HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
  if (process == nullptr) {
    return false;
  }

  HMODULE modules[kProcessModuleBufferSize];
  DWORD needed = 0;
  const bool success =
      EnumProcessModulesEx(process, modules, sizeof(modules), &needed, LIST_MODULES_ALL) != FALSE;
  if (success) {
    const DWORD count = qMin<DWORD>(needed / sizeof(HMODULE), kProcessModuleBufferSize);
    wchar_t modulePath[MAX_PATH];
    for (DWORD i = 0; i < count; ++i) {
      if (GetModuleFileNameExW(process, modules[i], modulePath, MAX_PATH) != 0) {
        if (QString::fromWCharArray(modulePath).contains(moduleSubstring, Qt::CaseInsensitive)) {
          CloseHandle(process);
          return true;
        }
      }
    }
  }

  CloseHandle(process);
  return false;
}

bool injectDllIntoProcessOnce(DWORD processId, const QString &dllPath, QString *errorMessage) {
  const std::wstring wideDllPath = QDir::toNativeSeparators(dllPath).toStdWString();
  const SIZE_T bytes = (wideDllPath.size() + 1) * sizeof(wchar_t);

  HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                   PROCESS_VM_WRITE | PROCESS_VM_READ,
                               FALSE, processId);
  if (process == nullptr) {
    if (errorMessage != nullptr) {
      *errorMessage = formatWindowsErrorMessage(GetLastError());
    }
    return false;
  }

  LPVOID remoteMemory = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (remoteMemory == nullptr) {
    if (errorMessage != nullptr) {
      *errorMessage = formatWindowsErrorMessage(GetLastError());
    }
    CloseHandle(process);
    return false;
  }

  const BOOL wroteMemory = WriteProcessMemory(process, remoteMemory, wideDllPath.c_str(), bytes, nullptr);
  if (!wroteMemory) {
    if (errorMessage != nullptr) {
      *errorMessage = formatWindowsErrorMessage(GetLastError());
    }
    VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(process);
    return false;
  }

  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  auto loadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW"));
  if (loadLibraryW == nullptr) {
    if (errorMessage != nullptr) {
      *errorMessage = "Could not locate LoadLibraryW";
    }
    VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(process);
    return false;
  }

  HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibraryW, remoteMemory, 0, nullptr);
  if (thread == nullptr) {
    if (errorMessage != nullptr) {
      *errorMessage = formatWindowsErrorMessage(GetLastError());
    }
    VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(process);
    return false;
  }

  WaitForSingleObject(thread, 5000);
  DWORD exitCode = 0;
  GetExitCodeThread(thread, &exitCode);

  CloseHandle(thread);
  VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);
  CloseHandle(process);

  if (exitCode == 0) {
    if (errorMessage != nullptr) {
      *errorMessage = QString("LoadLibraryW returned 0 for %1").arg(QFileInfo(dllPath).fileName());
    }
    return false;
  }
  return true;
}

bool injectDllWithRetries(DWORD processId, const QString &dllPath, QString *errorMessage) {
  QString lastError;
  for (int attempt = 0; attempt < kInjectionMaxRetries; ++attempt) {
    if (injectDllIntoProcessOnce(processId, dllPath, &lastError)) {
      return true;
    }
    Sleep(kInjectionRetryDelayMs);
  }

  for (int waitAttempt = 0; waitAttempt < kInjectionMaxRetries; ++waitAttempt) {
    if (processHasModule(processId, "vorbis")) {
      for (int attempt = 0; attempt < kInjectionMaxRetries; ++attempt) {
        if (injectDllIntoProcessOnce(processId, dllPath, &lastError)) {
          return true;
        }
        Sleep(kInjectionRetryDelayMs);
      }
      break;
    }
    Sleep(kInjectionRetryDelayMs);
  }

  if (errorMessage != nullptr) {
    *errorMessage = lastError.isEmpty() ? QString("DLL injection failed for %1").arg(dllPath) : lastError;
  }
  return false;
}
#endif

class FavoritesStore {
 public:
  FavoritesStore() { load(); }

  void load() {
    records_.clear();
    loadFromFile(path(), true);
  }

  bool save() const {
    return saveToFile(path());
  }

  QList<FavoriteRecord> records() const { return records_.values(); }

  bool contains(const QString &key) const { return records_.contains(key); }

  FavoriteRecord recordForKey(const QString &key) const { return records_.value(key); }

  void addOrUpdate(const ServerData &server) {
    auto record = records_.value(serverKey(server.host, server.port));
    record.name = server.hostname;
    record.host = server.host;
    record.port = server.port;
    records_.insert(serverKey(server.host, server.port), record);
  }

  void addOrUpdate(const FavoriteRecord &record) { records_.insert(serverKey(record.host, record.port), record); }

  void remove(const QString &key) { records_.remove(key); }

  bool saveToFile(const QString &filePath, bool includeSecrets = true) const {
    const auto dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
      return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }

    if (file.write(QJsonDocument(toJsonArray(includeSecrets)).toJson(QJsonDocument::Indented)) == -1) {
      return false;
    }

    return file.commit();
  }

  bool loadFromFile(const QString &filePath, bool merge) {
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
      return false;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
      return false;
    }

    if (!merge) {
      records_.clear();
    }

    for (const auto &entry : document.array()) {
      FavoriteRecord record;
      if (!favoriteFromJson(entry, &record)) {
        continue;
      }
      records_.insert(serverKey(record.host, record.port), record);
    }
    return true;
  }

 private:
  static bool favoriteFromJson(const QJsonValue &entry, FavoriteRecord *record) {
    if (!entry.isObject() || record == nullptr) {
      return false;
    }

    const auto object = entry.toObject();
    record->name = object.value("serverName").toString(object.value("name").toString());
    record->alias = object.value("alias").toString();
    record->host = object.value("host").toString();
    record->port = static_cast<quint16>(object.value("port").toInt(kDefaultPort));
    record->serverPassword = object.value("serverPassword").toString();
    return !record->host.isEmpty();
  }

  QJsonArray toJsonArray(bool includeSecrets) const {
    QJsonArray array;
    for (const auto &record : records_) {
      QJsonObject object;
      object.insert("name", record.name);
      object.insert("serverName", record.name);
      object.insert("alias", record.alias);
      object.insert("host", record.host);
      object.insert("port", static_cast<int>(record.port));
      if (includeSecrets && !record.serverPassword.isEmpty()) {
        object.insert("serverPassword", record.serverPassword);
      }
      array.append(object);
    }
    return array;
  }

  QString path() const {
    const auto baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return baseDir + "/favorites.json";
  }

  QHash<QString, FavoriteRecord> records_;
};

class SettingsStore {
 public:
  SettingsStore() { load(); }

  void load() {
    settings_ = {};

    QFile file(path());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
      return;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
      return;
    }

    const auto object = document.object();
    settings_.autoSaveServerPasswords = object.value("autoSaveServerPasswords").toBool(false);
    settings_.gamePath = object.value("gamePath").toString();
    settings_.ompLauncherPath = object.value("ompLauncherPath").toString();
  }

  bool save() const {
    const auto dir = QFileInfo(path()).absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
      return false;
    }

    QJsonObject object;
    object.insert("autoSaveServerPasswords", settings_.autoSaveServerPasswords);
    object.insert("gamePath", settings_.gamePath);
    object.insert("ompLauncherPath", settings_.ompLauncherPath);

    QSaveFile file(path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }

    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) == -1) {
      return false;
    }

    return file.commit();
  }

  const LauncherSettings &settings() const { return settings_; }
  void setSettings(const LauncherSettings &settings) { settings_ = settings; }

 private:
  QString path() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath("launcher_settings.json");
  }

  LauncherSettings settings_;
};

class PingChartWidget final : public QWidget {
 public:
  explicit PingChartWidget(QWidget *parent = nullptr) : QWidget(parent) { setMinimumSize(260, 96); }

  void setSamples(const QVector<int> &samples) {
    samples_ = samples;
    update();
  }

 protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(15, 15, 15));
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int maxValue = 200;
    const QRect plot = rect().adjusted(36, 8, -8, -12);

    painter.setPen(QColor(170, 170, 170));
    painter.setFont(QFont("Sans Serif", 7));
    for (int i = 0; i <= 4; ++i) {
      const int y = plot.top() + (plot.height() * i) / 4;
      const int value = maxValue - (maxValue * i) / 4;
      painter.drawText(QRect(2, y - 8, 30, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(value));
    }
    painter.drawText(QRect(2, plot.bottom() + 1, 30, 10), Qt::AlignRight | Qt::AlignVCenter, "ms");

    painter.setPen(QColor(70, 70, 70));
    painter.drawRect(plot);
    for (int i = 0; i <= 4; ++i) {
      const int y = plot.top() + (plot.height() * i) / 4;
      painter.drawLine(plot.left(), y, plot.right(), y);
    }

    if (samples_.isEmpty()) {
      return;
    }

    painter.setPen(QPen(QColor(255, 153, 0), 2));
    QPainterPath path;
    for (int i = 0; i < samples_.size(); ++i) {
      const qreal x = plot.left() + (plot.width() * i) / qMax(1, samples_.size() - 1);
      const int clamped = qBound(0, samples_[i], maxValue);
      const qreal y = plot.bottom() - (plot.height() * clamped) / maxValue;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    painter.drawPath(path);
  }

 private:
  QVector<int> samples_;
};

class SampQueryJob final : public QObject {
 public:
  SampQueryJob(QString host, quint16 port, QueryOptions options, QObject *parent = nullptr)
      : QObject(parent), host_(std::move(host)), port_(port), options_(options) {}

  QString key() const { return serverKey(host_, port_); }
  const QueryResult &result() const { return result_; }
  void setFinishedCallback(std::function<void()> callback) { finishedCallback_ = std::move(callback); }

  void start() {
    if (!resolveAddress()) {
      result_.error = "Unable to resolve hostname";
      finish();
      return;
    }

    if (!socket_.bind(QHostAddress::AnyIPv4, 0)) {
      result_.error = socket_.errorString();
      finish();
      return;
    }

    connect(&socket_, &QUdpSocket::readyRead, this, &SampQueryJob::onReadyRead);
    timeoutTimer_.setSingleShot(true);
    timeoutTimer_.setInterval(kQueryTimeoutMs);
    connect(&timeoutTimer_, &QTimer::timeout, this, &SampQueryJob::onTimeout);

    if (options_.info) {
      pending_.insert('i');
      socket_.writeDatagram(buildQueryPacket(address_, port_, 'i'), address_, port_);
    }
    if (options_.players) {
      pending_.insert('c');
      socket_.writeDatagram(buildQueryPacket(address_, port_, 'c'), address_, port_);
    }
    if (options_.rules) {
      pending_.insert('r');
      socket_.writeDatagram(buildQueryPacket(address_, port_, 'r'), address_, port_);
    }
    if (options_.extraInfo) {
      pending_.insert('o');
      socket_.writeDatagram(buildQueryPacket(address_, port_, 'o'), address_, port_);
    }
    if (options_.ping) {
      pending_.insert('p');
      pingTimer_.start();
      socket_.writeDatagram(buildQueryPacket(address_, port_, 'p'), address_, port_);
    }

    if (pending_.isEmpty()) {
      finish();
      return;
    }

    timeoutTimer_.start();
  }

 private:
  void finish() {
    timeoutTimer_.stop();
    if (finishedCallback_) {
      finishedCallback_();
    }
  }

  bool resolveAddress() {
    QHostAddress parsedAddress;
    if (parsedAddress.setAddress(host_)) {
      address_ = parsedAddress;
      return parsedAddress.protocol() == QAbstractSocket::IPv4Protocol;
    }

    const auto hostInfo = QHostInfo::fromName(host_);
    for (const auto &candidate : hostInfo.addresses()) {
      if (candidate.protocol() == QAbstractSocket::IPv4Protocol) {
        address_ = candidate;
        return true;
      }
    }
    return false;
  }

  void onReadyRead() {
    while (socket_.hasPendingDatagrams()) {
      QByteArray datagram;
      datagram.resize(static_cast<int>(socket_.pendingDatagramSize()));
      socket_.readDatagram(datagram.data(), datagram.size());
      if (datagram.size() < 11) {
        continue;
      }

      const char type = datagram.at(10);
      const QByteArray payload = datagram.mid(11);
      switch (type) {
        case 'i':
          parseInfoPacket(payload);
          pending_.remove('i');
          break;
        case 'c':
          parsePlayersPacket(payload);
          pending_.remove('c');
          break;
        case 'r':
          parseRulesPacket(payload);
          pending_.remove('r');
          break;
        case 'o':
          result_.extraInfoReceived = true;
          result_.success = true;
          pending_.remove('o');
          break;
        case 'p':
          result_.pingReceived = true;
          result_.ping = static_cast<int>(pingTimer_.elapsed());
          result_.success = true;
          pending_.remove('p');
          break;
        default:
          break;
      }
    }

    if (pending_.isEmpty()) {
      finish();
    }
  }

  void onTimeout() {
    if (pending_.contains('p') && !result_.pingReceived) {
      result_.pingReceived = true;
      result_.ping = kPingTimeoutValue;
    }
    if (!result_.success && result_.error.isEmpty()) {
      result_.error = "Query timeout";
    }
    finish();
  }

  void parseInfoPacket(const QByteArray &payload) {
    QByteArray buffer = payload;
    QDataStream stream(&buffer, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    qint8 passwordFlag = 0;
    quint16 players = 0;
    quint16 maxPlayers = 0;
    quint32 hostnameLength = 0;
    quint32 modeLength = 0;
    quint32 languageLength = 0;
    stream >> passwordFlag >> players >> maxPlayers >> hostnameLength;
    if (stream.status() != QDataStream::Ok || hostnameLength > 4096) {
      return;
    }

    QByteArray hostname(hostnameLength, '\0');
    if (stream.readRawData(hostname.data(), hostname.size()) != hostname.size()) {
      return;
    }
    stream >> modeLength;
    if (modeLength > 4096) {
      return;
    }
    QByteArray mode(modeLength, '\0');
    if (stream.readRawData(mode.data(), mode.size()) != mode.size()) {
      return;
    }
    stream >> languageLength;
    if (languageLength > 4096) {
      return;
    }
    QByteArray language(languageLength, '\0');
    if (stream.readRawData(language.data(), language.size()) != language.size()) {
      return;
    }

    result_.locked = passwordFlag != 0;
    result_.players = players;
    result_.maxPlayers = maxPlayers;
    result_.hostname = decodeSampString(hostname);
    result_.gamemode = decodeSampString(mode);
    result_.language = decodeSampString(language);
    result_.infoReceived = true;
    result_.success = true;
  }

  void parsePlayersPacket(const QByteArray &payload) {
    QByteArray buffer = payload;
    QDataStream stream(&buffer, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 playerCount = 0;
    stream >> playerCount;
    if (stream.status() != QDataStream::Ok || playerCount > 1000) {
      return;
    }

    QVector<PlayerEntry> players;
    players.reserve(playerCount);
    for (quint16 i = 0; i < playerCount; ++i) {
      quint8 nameLength = 0;
      qint32 score = 0;
      stream >> nameLength;
      QByteArray name(nameLength, '\0');
      if (stream.readRawData(name.data(), name.size()) != name.size()) {
        return;
      }
      stream >> score;
      players.push_back(PlayerEntry{decodeSampString(name), static_cast<int>(score)});
    }

    result_.playerEntries = players;
    result_.playersReceived = true;
    result_.success = true;
  }

  void parseRulesPacket(const QByteArray &payload) {
    QByteArray buffer = payload;
    QDataStream stream(&buffer, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 ruleCount = 0;
    stream >> ruleCount;
    if (stream.status() != QDataStream::Ok || ruleCount > 2048) {
      return;
    }

    QVector<QPair<QString, QString>> rules;
    rules.reserve(ruleCount);
    for (quint16 i = 0; i < ruleCount; ++i) {
      quint8 keyLength = 0;
      quint8 valueLength = 0;
      stream >> keyLength;
      QByteArray key(keyLength, '\0');
      if (stream.readRawData(key.data(), key.size()) != key.size()) {
        return;
      }
      stream >> valueLength;
      QByteArray value(valueLength, '\0');
      if (stream.readRawData(value.data(), value.size()) != value.size()) {
        return;
      }
      rules.push_back({decodeSampString(key), decodeSampString(value)});
    }

    std::sort(rules.begin(), rules.end(), [](const auto &left, const auto &right) {
      return left.first < right.first;
    });

    result_.rules = rules;
    result_.rulesReceived = true;
    result_.success = true;
  }

  QString host_;
  quint16 port_;
  QueryOptions options_;
  QueryResult result_;
  QHostAddress address_;
  QUdpSocket socket_;
  QTimer timeoutTimer_;
  QElapsedTimer pingTimer_;
  QSet<char> pending_;
  std::function<void()> finishedCallback_;
};

class LauncherWindow final : public QMainWindow {
 public:
  LauncherWindow() {
    setWindowTitle("open.mp Classic");
    applyRuntimeWindowIcon(this);
    resize(960, 640);

    buildUi();
    QTimer::singleShot(0, this, [this]() { applyRuntimeWindowIcon(this); });
    detailRefreshTimer_.setInterval(1000);
    connect(&detailRefreshTimer_, &QTimer::timeout, this, [this]() { refreshSelectedServerDetails(); });
    loadFavoritesIntoState();
    refreshFromMaster();
  }

 protected:
  void showEvent(QShowEvent *event) override {
    QMainWindow::showEvent(event);
    applyRuntimeWindowIcon(this);
  }

 private:
  enum class ActiveTab { Favorites = 0, Internet = 1, Hosted = 2 };

  QHash<QString, ServerData> servers_;
  QStringList internetKeys_;
  QStringList displayedKeys_;
  QNetworkAccessManager networkManager_;
  FavoritesStore favoritesStore_;
  SettingsStore settingsStore_;
  QQueue<QString> pingQueue_;
  QSet<QString> queuedPingKeys_;
  QHash<QString, QPointer<SampQueryJob>> pingJobs_;
  QPointer<SampQueryJob> detailJob_;
  QTimer detailRefreshTimer_;
  QString activeDetailKey_;

  QTableWidget *serverTable_ = nullptr;
  QTableWidget *playersTable_ = nullptr;
  QTableWidget *rulesTable_ = nullptr;
  QLineEdit *nameEdit_ = nullptr;
  QTabBar *tabs_ = nullptr;
  QLineEdit *modeFilterEdit_ = nullptr;
  QLineEdit *languageFilterEdit_ = nullptr;
  QLineEdit *hostnameFilterEdit_ = nullptr;
  QCheckBox *notFullCheck_ = nullptr;
  QCheckBox *notEmptyCheck_ = nullptr;
  QCheckBox *noPasswordCheck_ = nullptr;
  QLabel *infoTitle_ = nullptr;
  QLabel *addressValue_ = nullptr;
  QLabel *playersValue_ = nullptr;
  QLabel *pingValue_ = nullptr;
  QLabel *modeValue_ = nullptr;
  QLabel *languageValue_ = nullptr;
  PingChartWidget *pingChart_ = nullptr;
  QAction *addServerAction_ = nullptr;
  QAction *addFavoriteAction_ = nullptr;
  QAction *removeFavoriteAction_ = nullptr;
  QAction *connectAction_ = nullptr;
  QAction *serverPropertiesAction_ = nullptr;
  QAction *settingsAction_ = nullptr;
  QAction *aboutAction_ = nullptr;
  QAction *refreshAction_ = nullptr;

  static QTableWidgetItem *makeItem(
      const QString &text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(alignment);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
  }

  static SortableTableItem *makeSortableItem(
      const QString &text, const QVariant &sortValue,
      Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
    auto *item = new SortableTableItem(text, sortValue);
    item->setTextAlignment(alignment);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
  }

  static SortableTableItem *makeIconSortableItem(
      const QIcon &icon, const QString &toolTip, const QVariant &sortValue,
      Qt::Alignment alignment = Qt::AlignCenter) {
    auto *item = makeSortableItem(QString(), sortValue, alignment);
    item->setIcon(icon);
    item->setToolTip(toolTip);
    return item;
  }

  ActiveTab activeTab() const { return static_cast<ActiveTab>(tabs_->currentIndex()); }

  void buildUi() {
    buildMenus();
    buildToolbar();

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(4, 2, 4, 0);
    rootLayout->setSpacing(4);

    auto *topSplitter = new QSplitter(Qt::Horizontal, central);
    topSplitter->addWidget(buildServerPanel(topSplitter));
    topSplitter->addWidget(buildSidebar(topSplitter));
    topSplitter->setStretchFactor(0, 4);
    topSplitter->setStretchFactor(1, 1);

    auto *bottomPanel = new QWidget(central);
    auto *bottomLayout = new QHBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(6);
    bottomLayout->addWidget(buildFilterPanel(bottomPanel), 0);
    bottomLayout->addWidget(buildInfoPanel(bottomPanel), 1);

    tabs_ = new QTabBar(central);
    tabs_->addTab("Favorites");
    tabs_->addTab("Internet");
    tabs_->addTab("Hosted");
    tabs_->setCurrentIndex(static_cast<int>(ActiveTab::Internet));
    tabs_->setExpanding(false);
    tabs_->setDrawBase(true);
    connect(tabs_, &QTabBar::currentChanged, this, [this](int) {
      updateTabActions();
      populateServerTable();
      if (activeTab() == ActiveTab::Favorites) {
        enqueuePingQueries(currentSourceKeys());
      }
    });
    updateTabActions();

    rootLayout->addWidget(topSplitter, 1);
    rootLayout->addWidget(bottomPanel, 0);
    rootLayout->addWidget(tabs_, 0);

    setCentralWidget(central);
    statusBar()->showMessage("Fetching server list from open.mp API...");
  }

  void buildMenus() {
    const QColor iconColor = actionIconColor(this);
    connectAction_ = new QAction(makePlayActionIcon(iconColor), "Connect", this);
    addServerAction_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder), "Add Favorite Server...", this);
    addFavoriteAction_ = new QAction(favoriteStateIcon(), "Add To Favorites", this);
    removeFavoriteAction_ = new QAction(makeCloseActionIcon(iconColor), "Remove From Favorites", this);
    serverPropertiesAction_ = new QAction(makeServerPropertiesIcon(iconColor), "Server Properties", this);
    refreshAction_ = new QAction(makeRefreshActionIcon(iconColor), "Refresh", this);
    settingsAction_ = new QAction(makeSettingsIcon(iconColor), "Settings", this);
    aboutAction_ = new QAction(makeInfoActionIcon(iconColor), "About", this);

    connect(connectAction_, &QAction::triggered, this, [this]() { connectSelectedServer(); });
    connect(addServerAction_, &QAction::triggered, this, [this]() { addFavoriteServerManually(); });
    connect(addFavoriteAction_, &QAction::triggered, this, [this]() { addSelectedToFavorites(); });
    connect(removeFavoriteAction_, &QAction::triggered, this, [this]() { removeSelectedFromFavorites(); });
    connect(serverPropertiesAction_, &QAction::triggered, this, [this]() { showServerPropertiesDialog(); });
    connect(refreshAction_, &QAction::triggered, this, [this]() { refreshFromMaster(); });
    connect(settingsAction_, &QAction::triggered, this, [this]() { showSettingsDialog(); });
    connect(aboutAction_, &QAction::triggered, this, [this]() {
      QMessageBox::information(this, "About open.mp Classic",
                               "open.mp Classic\n\nQt frontend with open.mp server list and SA:MP query backend.");
    });

    connectAction_->setEnabled(false);
    addServerAction_->setEnabled(false);
    addFavoriteAction_->setEnabled(false);
    removeFavoriteAction_->setEnabled(false);
    serverPropertiesAction_->setEnabled(false);

    auto *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Import Favorites...", this, [this]() { importFavorites(); });
    fileMenu->addAction("Export Favorites...", this, [this]() { exportFavorites(); });
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction(refreshAction_);

    auto *serversMenu = menuBar()->addMenu("Servers");
    serversMenu->addAction(addServerAction_);
    serversMenu->addSeparator();
    serversMenu->addAction(connectAction_);
    serversMenu->addAction(addFavoriteAction_);
    serversMenu->addAction(removeFavoriteAction_);
    serversMenu->addAction(serverPropertiesAction_);

    auto *toolsMenu = menuBar()->addMenu("Tools");
    toolsMenu->addAction(settingsAction_);
    toolsMenu->addAction("Reload Favorites", this, [this]() {
      loadFavoritesIntoState();
      populateServerTable();
    });

    auto *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction(aboutAction_);
  }

  void buildToolbar() {
    auto *toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    toolbar->addAction(connectAction_);
    toolbar->addAction(addServerAction_);
    toolbar->addAction(addFavoriteAction_);
    toolbar->addAction(removeFavoriteAction_);
    toolbar->addAction(serverPropertiesAction_);
    toolbar->addSeparator();
    toolbar->addAction(settingsAction_);
    toolbar->addAction(refreshAction_);
    toolbar->addAction(aboutAction_);
    toolbar->addSeparator();

    auto *nameLabel = new QLabel("Name:");
    toolbar->addWidget(nameLabel);

    nameEdit_ = new QLineEdit;
    nameEdit_->setFixedWidth(160);
    nameEdit_->setText("Knogle");
    toolbar->addWidget(nameEdit_);

    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
  }

  QWidget *buildServerPanel(QWidget *parent) {
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    serverTable_ = new QTableWidget(0, 7, panel);
    serverTable_->setHorizontalHeaderLabels({"*", "", "Hostname", "Players", "Ping", "Mode", "Language"});
    auto *lockHeaderItem = new QTableWidgetItem;
    lockHeaderItem->setIcon(headerLockIcon());
    lockHeaderItem->setToolTip("Locked");
    lockHeaderItem->setTextAlignment(Qt::AlignCenter);
    serverTable_->setHorizontalHeaderItem(1, lockHeaderItem);
    serverTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    serverTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    serverTable_->setAlternatingRowColors(true);
    serverTable_->setShowGrid(true);
    serverTable_->verticalHeader()->hide();
    serverTable_->verticalHeader()->setDefaultSectionSize(22);
    serverTable_->horizontalHeader()->setStretchLastSection(false);
    serverTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    serverTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    serverTable_->setColumnWidth(0, 26);
    serverTable_->setColumnWidth(1, 26);
    serverTable_->setColumnWidth(3, 60);
    serverTable_->setColumnWidth(4, 52);
    serverTable_->setColumnWidth(5, 180);
    serverTable_->setColumnWidth(6, 110);
    serverTable_->setSortingEnabled(true);
    serverTable_->horizontalHeader()->setSectionsClickable(true);
    serverTable_->horizontalHeader()->setSortIndicatorShown(true);
    serverTable_->sortItems(2, Qt::AscendingOrder);
    connect(serverTable_, &QTableWidget::itemSelectionChanged, this, [this]() { updateSelection(); });
    connect(serverTable_, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *) {
      showServerPropertiesDialog();
    });

    layout->addWidget(serverTable_);
    return panel;
  }

  QWidget *buildSidebar(QWidget *parent) {
    auto *splitter = new QSplitter(Qt::Vertical, parent);

    playersTable_ = new QTableWidget(0, 2, splitter);
    playersTable_->setHorizontalHeaderLabels({"Player", "Score"});
    playersTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playersTable_->setSelectionMode(QAbstractItemView::NoSelection);
    playersTable_->setAlternatingRowColors(true);
    playersTable_->verticalHeader()->hide();
    playersTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    playersTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    playersTable_->setColumnWidth(1, 58);

    rulesTable_ = new QTableWidget(0, 2, splitter);
    rulesTable_->setHorizontalHeaderLabels({"Rule", "Value"});
    rulesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rulesTable_->setSelectionMode(QAbstractItemView::NoSelection);
    rulesTable_->setAlternatingRowColors(true);
    rulesTable_->verticalHeader()->hide();
    rulesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    rulesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({260, 120});
    return splitter;
  }

  QWidget *buildFilterPanel(QWidget *parent) {
    auto *group = new QGroupBox("Filter", parent);
    group->setFixedWidth(300);

    auto *layout = new QGridLayout(group);
    layout->addWidget(new QLabel("Mode:"), 0, 0);
    modeFilterEdit_ = new QLineEdit;
    layout->addWidget(modeFilterEdit_, 0, 1);
    notFullCheck_ = new QCheckBox("Not Full");
    layout->addWidget(notFullCheck_, 0, 2);

    layout->addWidget(new QLabel("Language:"), 1, 0);
    languageFilterEdit_ = new QLineEdit;
    layout->addWidget(languageFilterEdit_, 1, 1);
    notEmptyCheck_ = new QCheckBox("Not Empty");
    layout->addWidget(notEmptyCheck_, 1, 2);

    layout->addWidget(new QLabel("Hostname:"), 2, 0);
    hostnameFilterEdit_ = new QLineEdit;
    layout->addWidget(hostnameFilterEdit_, 2, 1);
    noPasswordCheck_ = new QCheckBox("No Passwd");
    layout->addWidget(noPasswordCheck_, 2, 2);
    layout->setColumnStretch(1, 1);

    const auto repopulate = [this]() { populateServerTable(); };
    connect(modeFilterEdit_, &QLineEdit::textChanged, this, repopulate);
    connect(languageFilterEdit_, &QLineEdit::textChanged, this, repopulate);
    connect(hostnameFilterEdit_, &QLineEdit::textChanged, this, repopulate);
    connect(notFullCheck_, &QCheckBox::toggled, this, repopulate);
    connect(notEmptyCheck_, &QCheckBox::toggled, this, repopulate);
    connect(noPasswordCheck_, &QCheckBox::toggled, this, repopulate);
    return group;
  }

  QWidget *buildInfoPanel(QWidget *parent) {
    auto *group = new QGroupBox(parent);
    group->setTitle("");
    auto *layout = new QGridLayout(group);

    infoTitle_ = new QLabel("Server Info");
    layout->addWidget(infoTitle_, 0, 0, 1, 3);

    layout->addWidget(new QLabel("Address:"), 1, 0);
    addressValue_ = new QLabel("-");
    layout->addWidget(addressValue_, 1, 1);

    layout->addWidget(new QLabel("Players:"), 2, 0);
    playersValue_ = new QLabel("-");
    layout->addWidget(playersValue_, 2, 1);

    layout->addWidget(new QLabel("Ping:"), 3, 0);
    pingValue_ = new QLabel("-");
    layout->addWidget(pingValue_, 3, 1);

    layout->addWidget(new QLabel("Mode:"), 4, 0);
    modeValue_ = new QLabel("-");
    layout->addWidget(modeValue_, 4, 1);

    layout->addWidget(new QLabel("Language:"), 5, 0);
    languageValue_ = new QLabel("-");
    layout->addWidget(languageValue_, 5, 1);

    auto *chartLabel = new QLabel("Ping");
    chartLabel->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    pingChart_ = new PingChartWidget(group);
    layout->addWidget(chartLabel, 1, 2);
    layout->addWidget(pingChart_, 2, 2, 4, 1);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 2);
    return group;
  }

  void showServerPropertiesDialog(const QString &requestedKey = {}) {
    const auto key = requestedKey.isEmpty() ? currentSelectedKey() : requestedKey;
    if (key.isEmpty() || !servers_.contains(key)) {
      return;
    }

    const auto server = servers_.value(key);
    auto record = favoritesStore_.recordForKey(key);
    if (record.host.isEmpty()) {
      record.name = server.hostname;
      record.host = server.host;
      record.port = server.port;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Server Properties");
    dialog.setModal(true);
    dialog.resize(380, 220);

    auto *rootLayout = new QVBoxLayout(&dialog);

    auto *titleLabel = new QLabel(server.hostname.isEmpty() ? key : server.hostname, &dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(titleLabel);

    auto *infoGrid = new QGridLayout;
    infoGrid->addWidget(new QLabel("Address:", &dialog), 0, 0);
    infoGrid->addWidget(new QLabel(serverKey(server.host, server.port), &dialog), 0, 1);
    infoGrid->addWidget(new QLabel("Players:", &dialog), 1, 0);
    infoGrid->addWidget(new QLabel(QString("%1 / %2").arg(server.players).arg(server.maxPlayers), &dialog), 1, 1);
    infoGrid->addWidget(new QLabel("Ping:", &dialog), 2, 0);
    infoGrid->addWidget(new QLabel(server.ping < 0 ? "9999" : QString::number(server.ping), &dialog), 2, 1);
    infoGrid->addWidget(new QLabel("Mode:", &dialog), 3, 0);
    infoGrid->addWidget(new QLabel(server.mode.isEmpty() ? "-" : server.mode, &dialog), 3, 1);
    infoGrid->addWidget(new QLabel("Language:", &dialog), 4, 0);
    infoGrid->addWidget(new QLabel(server.language.isEmpty() ? "-" : server.language, &dialog), 4, 1);
    infoGrid->setColumnStretch(1, 1);
    rootLayout->addLayout(infoGrid);

    auto *passwordGrid = new QGridLayout;
    auto *serverPasswordEdit = new QLineEdit(record.serverPassword, &dialog);
    serverPasswordEdit->setEchoMode(QLineEdit::Password);
    passwordGrid->addWidget(new QLabel("Server Password:", &dialog), 0, 0);
    passwordGrid->addWidget(serverPasswordEdit, 0, 1);
    passwordGrid->setColumnStretch(1, 1);
    rootLayout->addLayout(passwordGrid);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    auto *connectButton = new QPushButton("Connect", &dialog);
    auto *saveButton = new QPushButton("Save", &dialog);
    auto *cancelButton = new QPushButton("Cancel", &dialog);
    connectButton->setDefault(true);
    connectButton->setAutoDefault(true);
    buttonRow->addWidget(connectButton);
    buttonRow->addWidget(saveButton);
    buttonRow->addWidget(cancelButton);
    rootLayout->addLayout(buttonRow);

    const auto persistFavoriteRecord = [this, key, server, serverPasswordEdit]() {
      FavoriteRecord updated = favoritesStore_.recordForKey(key);
      updated.name = isUnresolvedFavoriteHostname(server.hostname, server.host, server.port) ? QString() : server.hostname;
      updated.host = server.host;
      updated.port = server.port;
      updated.serverPassword = serverPasswordEdit->text();
      favoritesStore_.addOrUpdate(updated);
      if (!favoritesStore_.save()) {
        QMessageBox::warning(this, "Save Failed", "Could not save server properties.");
        return false;
      }

      auto &storedServer = servers_[key];
      storedServer.favorite = true;
      storedServer.hostname = server.hostname;
      populateServerTable();
      updateFavoriteActions(storedServer);
      return true;
    };

    connect(saveButton, &QPushButton::clicked, &dialog, [&dialog, persistFavoriteRecord]() {
      if (persistFavoriteRecord()) {
        dialog.accept();
      }
    });

    connect(connectButton, &QPushButton::clicked, &dialog, [this, key, persistFavoriteRecord]() {
      const auto settings = settingsStore_.settings();
      if (settings.autoSaveServerPasswords && !persistFavoriteRecord()) {
        return;
      }
      connectToServer(key);
    });

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connectButton->setFocus(Qt::TabFocusReason);
    dialog.exec();
  }

  void connectSelectedServer() {
    const auto key = currentSelectedKey();
    if (key.isEmpty() || !servers_.contains(key)) {
      return;
    }
    connectToServer(key);
  }

  void addFavoriteServerManually() {
    bool ok = false;
    const QString endpoint =
        QInputDialog::getText(this, "Add Favorite Server", "Enter server HOST:PORT", QLineEdit::Normal, {}, &ok);
    if (!ok) {
      return;
    }

    QString host;
    quint16 port = kDefaultPort;
    if (!parseEndpoint(endpoint.trimmed(), &host, &port)) {
      QMessageBox::warning(this, "Invalid Address", "Enter the server as HOST:PORT.");
      return;
    }

    const QString key = serverKey(host, port);
    const bool hadExistingServer = servers_.contains(key);
    const ServerData previousServer = servers_.value(key);
    ServerData server = servers_.value(key);
    server.favorite = true;
    server.host = host;
    server.port = port;
    if (server.hostname.isEmpty() || server.hostname == key) {
      server.hostname = unresolvedFavoriteHostname(host, port);
    }
    servers_.insert(key, server);

    FavoriteRecord record;
    record.host = host;
    record.port = port;
    record.serverPassword = favoritesStore_.recordForKey(key).serverPassword;
    favoritesStore_.addOrUpdate(record);
    if (!favoritesStore_.save()) {
      if (hadExistingServer) {
        servers_.insert(key, previousServer);
      } else {
        servers_.remove(key);
      }
      QMessageBox::warning(this, "Save Failed", "Could not save favorites.");
      return;
    }

    populateServerTable();
    enqueuePingQueries({key});
    const int row = rowForKey(key);
    if (row >= 0) {
      serverTable_->selectRow(row);
    }
    startDetailQuery(key);
    statusBar()->showMessage(QString("Added favorite server %1").arg(key), 4000);
  }

  QString resolveConfiguredLauncherPath(const LauncherSettings &settings) const {
    const QString configured = settings.ompLauncherPath.trimmed();
    const auto resolveCandidate = [](const QString &candidate) -> QString {
      if (candidate.isEmpty()) {
        return {};
      }

      QFileInfo info(candidate);
      if (info.exists() && info.isFile()) {
        return info.absoluteFilePath();
      }
      if (info.exists() && info.isDir()) {
        for (const auto &name : {QStringLiteral("omp-launcher.exe"), QStringLiteral("omp-launcher")}) {
          const QString insidePath = QDir(info.absoluteFilePath()).filePath(name);
          if (QFileInfo::exists(insidePath)) {
            return QFileInfo(insidePath).absoluteFilePath();
          }
        }
      }
      return {};
    };

    if (const QString resolved = resolveCandidate(configured); !resolved.isEmpty()) {
      return resolved;
    }

    for (const auto &baseDir : {settings.gamePath.trimmed(), QCoreApplication::applicationDirPath()}) {
      if (baseDir.isEmpty()) {
        continue;
      }
      if (const QString resolved = resolveCandidate(baseDir); !resolved.isEmpty()) {
        return resolved;
      }
    }

    return {};
  }

  QString resolveInjectHelperPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("omp-launcher-classic-inject-helper.exe"),
        QDir(appDir).filePath("omp-launcher-classic-inject-helper"),
        QDir(appDir).filePath("../omp-launcher-classic-inject-helper.exe"),
    };

    for (const auto &candidate : candidates) {
      QFileInfo info(QDir::cleanPath(candidate));
      if (info.exists() && info.isFile()) {
        return info.absoluteFilePath();
      }
    }
    return {};
  }

  QString resolveSampDllPath(const LauncherSettings &settings) const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("samp.dll"),
        QDir(appDir).filePath("client/samp.dll"),
        QDir(appDir).filePath("../client/samp.dll"),
        QDir(settings.gamePath).filePath("samp.dll"),
    };

    for (const auto &candidate : candidates) {
      QFileInfo info(QDir::cleanPath(candidate));
      if (info.exists() && info.isFile()) {
        return info.absoluteFilePath();
      }
    }
    return {};
  }

  QString resolveOmpClientDllPath(const LauncherSettings &settings) const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QStringList candidates = {
        QDir(appDir).filePath("omp-client.dll"),
        QDir(settings.gamePath).filePath("omp-client.dll"),
        localAppData.isEmpty() ? QString() : QDir(localAppData).filePath("mp.open.launcher/omp/omp-client.dll"),
        QDir(appDir).filePath("omp/omp-client.dll"),
        QDir(appDir).filePath("../omp/omp-client.dll"),
    };

    for (const auto &candidate : candidates) {
      if (candidate.isEmpty()) {
        continue;
      }
      QFileInfo info(QDir::cleanPath(candidate));
      if (info.exists() && info.isFile()) {
        return info.absoluteFilePath();
      }
    }
    return {};
  }

  void connectToServer(const QString &key) {
    if (!servers_.contains(key)) {
      return;
    }

    const auto settings = settingsStore_.settings();
    if (settings.gamePath.trimmed().isEmpty()) {
      QMessageBox::warning(this, "Connect Failed",
                           "No GTA: San Andreas path is configured.\n\nSet it in Settings first.");
      return;
    }

    const QString gameExePath = QDir(settings.gamePath).filePath("gta_sa.exe");
    if (!QFileInfo::exists(gameExePath)) {
      QMessageBox::warning(this, "Connect Failed",
                           QString("Could not find gta_sa.exe in:\n%1").arg(settings.gamePath));
      return;
    }

    const auto &server = servers_[key];
    const auto favoriteRecord = favoritesStore_.recordForKey(key);
    const QString nickname = nameEdit_->text().trimmed();
    if (nickname.isEmpty()) {
      QMessageBox::warning(this, "Connect Failed", "Nickname cannot be empty.");
      return;
    }

    QString password = favoriteRecord.serverPassword;
    if (server.locked && password.isEmpty()) {
      QMessageBox::warning(this, "Password Required",
                           "This server is locked. Open Server Properties and enter the server password first.");
      showServerPropertiesDialog(key);
      return;
    }

#ifdef Q_OS_WIN
    const QString injectHelperPath = resolveInjectHelperPath();
    if (injectHelperPath.isEmpty()) {
      QMessageBox::warning(
          this, "Connect Failed",
          "Could not find omp-launcher-classic-inject-helper.exe.\n\nPlace it next to the launcher executable.");
      return;
    }

    const QString sampDllPath = resolveSampDllPath(settings);
    if (sampDllPath.isEmpty()) {
      QMessageBox::warning(
          this, "Connect Failed",
          "Could not find a samp.dll to inject.\n\nPlace samp.dll next to the launcher, in a client subfolder, or in the GTA folder.");
      return;
    }

    const QString ompClientDllPath = resolveOmpClientDllPath(settings);
    if (ompClientDllPath.isEmpty()) {
      QMessageBox::warning(
          this, "Connect Failed",
          QString("Could not find omp-client.dll to inject.\n\n"
                  "Due to licensing, omp-client.dll must be downloaded manually.\n\n"
                  "Download:\n%1\n\n"
                  "Then place omp-client.dll next to the launcher executable.")
              .arg(kOmpClientDownloadUrl));
      return;
    }

    QStringList helperArguments = {
        "--name",
        nickname,
        "--host",
        server.host,
        "--port",
        QString::number(server.port),
        "--gamepath",
        QDir::toNativeSeparators(settings.gamePath),
        "--dll",
        QDir::toNativeSeparators(sampDllPath),
        "--omp-file",
        QDir::toNativeSeparators(ompClientDllPath),
    };
    QProcess helperProcess;
    helperProcess.setProgram(injectHelperPath);
    helperProcess.setWorkingDirectory(QFileInfo(injectHelperPath).absolutePath());
    helperProcess.setArguments(helperArguments);
    helperProcess.setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    if (!password.isEmpty()) {
      processEnvironment.insert(QString::fromLatin1(kHelperPasswordEnvVar), password);
    } else {
      processEnvironment.remove(QString::fromLatin1(kHelperPasswordEnvVar));
    }
    helperProcess.setProcessEnvironment(processEnvironment);

    helperProcess.start();
    if (!helperProcess.waitForStarted()) {
      QMessageBox::warning(this, "Connect Failed", "Failed to start omp-launcher-classic-inject-helper.exe.");
      return;
    }

    helperProcess.waitForFinished(-1);
    if (helperProcess.exitStatus() != QProcess::NormalExit || helperProcess.exitCode() != 0) {
      QString errorOutput = QString::fromLocal8Bit(helperProcess.readAll()).trimmed();
      if (errorOutput.isEmpty()) {
        errorOutput = "omp-launcher-classic-inject-helper.exe failed without an error message.";
      }
      QMessageBox::warning(this, "Connect Failed", errorOutput);
      return;
    }

    statusBar()->showMessage(
        QString("Launching %1 via omp-launcher-classic-inject-helper.exe ...")
            .arg(serverKey(server.host, server.port)),
        5000);
#else
    const QString launcherPath = resolveConfiguredLauncherPath(settings);
    if (launcherPath.isEmpty()) {
      QMessageBox::warning(
          this, "Connect Failed",
          "Could not find an open.mp launcher executable.\n\nSet it in Settings or place omp-launcher.exe in the GTA folder.");
      return;
    }

    QStringList arguments = {
        "--host", server.host, "--port", QString::number(server.port), "--name", nickname, "--gamepath",
        QDir::toNativeSeparators(settings.gamePath),
    };
    if (!password.isEmpty()) {
      arguments << "--password" << password;
    }

    if (!QProcess::startDetached(launcherPath, arguments, QFileInfo(launcherPath).absolutePath())) {
      QMessageBox::warning(this, "Connect Failed",
                           QString("Failed to start launcher:\n%1").arg(QDir::toNativeSeparators(launcherPath)));
      return;
    }

    statusBar()->showMessage(QString("Launching %1 via %2 ...")
                                 .arg(serverKey(server.host, server.port),
                                      QFileInfo(launcherPath).fileName()),
                             5000);
#endif
  }

  void showSettingsDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");
    dialog.setModal(true);
    dialog.resize(360, 180);

    auto settings = settingsStore_.settings();

    auto *rootLayout = new QVBoxLayout(&dialog);

    auto *passwordBox = new QGroupBox("Passwords", &dialog);
    auto *passwordLayout = new QVBoxLayout(passwordBox);
    auto *autoSaveServerCheck = new QCheckBox("Auto-Save Server Passwords", passwordBox);
    autoSaveServerCheck->setChecked(settings.autoSaveServerPasswords);
    passwordLayout->addWidget(autoSaveServerCheck);
    rootLayout->addWidget(passwordBox);

    auto *pathLayout = new QGridLayout;
    auto *pathLabel = new QLabel("San Andreas Installation Location:", &dialog);
    auto *pathEdit = new QLineEdit(settings.gamePath, &dialog);
    auto *browseButton = new QPushButton(style()->standardIcon(QStyle::SP_DirOpenIcon), QString(), &dialog);
    browseButton->setToolTip("Browse");
    pathLayout->addWidget(pathLabel, 0, 0, 1, 2);
    pathLayout->addWidget(pathEdit, 1, 0);
    pathLayout->addWidget(browseButton, 1, 1);
    pathLayout->setColumnStretch(0, 1);
    rootLayout->addLayout(pathLayout);

    connect(browseButton, &QPushButton::clicked, &dialog, [this, pathEdit]() {
      const auto selectedPath = QFileDialog::getExistingDirectory(this, "Select San Andreas Folder", pathEdit->text());
      if (!selectedPath.isEmpty()) {
        pathEdit->setText(selectedPath);
      }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            [&dialog, this, autoSaveServerCheck, pathEdit]() {
      LauncherSettings updated;
      updated.autoSaveServerPasswords = autoSaveServerCheck->isChecked();
      updated.gamePath = pathEdit->text().trimmed();
      settingsStore_.setSettings(updated);
      if (!settingsStore_.save()) {
        QMessageBox::warning(this, "Save Failed", "Could not save launcher settings.");
        return;
      }
      dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    rootLayout->addWidget(buttons);

    dialog.exec();
  }

  void importFavorites() {
    const QString filePath =
        QFileDialog::getOpenFileName(this, "Import Favorites", QString(), "JSON Files (*.json);;All Files (*)");
    if (filePath.isEmpty()) {
      return;
    }

    if (!favoritesStore_.loadFromFile(filePath, true)) {
      QMessageBox::warning(this, "Import Failed", "Could not import favorites from the selected JSON file.");
      return;
    }

    if (!favoritesStore_.save()) {
      QMessageBox::warning(this, "Import Failed", "Favorites were read, but could not be saved locally.");
      return;
    }

    loadFavoritesIntoState();
    populateServerTable();
    statusBar()->showMessage(QString("Imported favorites from %1").arg(QFileInfo(filePath).fileName()), 4000);
  }

  void exportFavorites() {
    QString filePath =
        QFileDialog::getSaveFileName(this, "Export Favorites", "favorites.json", "JSON Files (*.json);;All Files (*)");
    if (filePath.isEmpty()) {
      return;
    }
    if (!filePath.endsWith(".json", Qt::CaseInsensitive)) {
      filePath += ".json";
    }

    if (!favoritesStore_.saveToFile(filePath, false)) {
      QMessageBox::warning(this, "Export Failed", "Could not export favorites to the selected JSON file.");
      return;
    }

    statusBar()->showMessage(
        QString("Exported favorites to %1 without saved passwords").arg(QFileInfo(filePath).fileName()), 4000);
  }

  void loadFavoritesIntoState() {
    favoritesStore_.load();
    for (const auto &record : favoritesStore_.records()) {
      const auto key = serverKey(record.host, record.port);
      auto it = servers_.find(key);
      if (it == servers_.end()) {
        ServerData server;
        server.favorite = true;
        server.hostname = record.name.isEmpty() ? unresolvedFavoriteHostname(record.host, record.port) : record.name;
        server.host = record.host;
        server.port = record.port;
        it = servers_.insert(key, server);
      } else if (record.name.isEmpty() && (it->hostname.isEmpty() || it->hostname == key)) {
        it->hostname = unresolvedFavoriteHostname(record.host, record.port);
      }
      it->favorite = true;
    }

    for (auto it = servers_.begin(); it != servers_.end(); ++it) {
      it->favorite = favoritesStore_.contains(it.key());
    }
  }

  void refreshFromMaster() {
    statusBar()->showMessage("Refreshing server list from https://api.open.mp/servers/full ...");

    QNetworkRequest request(QUrl(QString::fromLatin1(kServerListUrl)));
    request.setTransferTimeout(30000);
    auto *reply = networkManager_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
      const QPointer<QNetworkReply> guard(reply);
      reply->deleteLater();
      if (!guard) {
        return;
      }

      if (reply->error() != QNetworkReply::NoError) {
        statusBar()->showMessage(QString("Server list refresh failed: %1").arg(reply->errorString()));
        populateServerTable();
        return;
      }

      const auto document = QJsonDocument::fromJson(reply->readAll());
      if (!document.isArray()) {
        statusBar()->showMessage("Server list refresh failed: invalid response format");
        return;
      }

      const auto oldServers = servers_;
      QStringList newInternetKeys;
      const auto serversArray = document.array();
      for (const auto &value : serversArray) {
        if (!value.isObject()) {
          continue;
        }

        const auto object = value.toObject();
        const auto core = object.value("core").toObject();
        QString host;
        quint16 port = kDefaultPort;
        if (!parseEndpoint(core.value("ip").toString(), &host, &port)) {
          continue;
        }

        const auto key = serverKey(host, port);
        ServerData server;
        server.host = host;
        server.port = port;
        server.hostname = core.value("hn").toString(key);
        server.mode = core.value("gm").toString();
        server.language = core.value("la").toString();
        server.locked = core.value("pa").toBool(false);
        server.players = core.value("pc").toInt(0);
        server.maxPlayers = core.value("pm").toInt(0);
        server.version = core.value("vn").toString();
        server.usingOmp = core.value("omp").toBool(false);
        server.partner = core.value("pr").toBool(false);
        server.favorite = favoritesStore_.contains(key);

        const auto rulesObject = object.value("ru").toObject();
        QVector<QPair<QString, QString>> rules;
        rules.reserve(rulesObject.size());
        for (auto it = rulesObject.begin(); it != rulesObject.end(); ++it) {
          rules.push_back({it.key(), it.value().toString()});
        }
        std::sort(rules.begin(), rules.end(), [](const auto &left, const auto &right) {
          return left.first < right.first;
        });
        server.rules = rules;
        server.usingOmp = server.usingOmp || isUsingOmp(server.rules, server.version);

        if (oldServers.contains(key)) {
          const auto &previous = oldServers[key];
          server.ping = previous.ping;
          server.pingSamples = previous.pingSamples;
          server.playerEntries = previous.playerEntries;
          server.lastQueryAt = previous.lastQueryAt;
          if (server.hostname.isEmpty()) {
            server.hostname = previous.hostname;
          }
        }

        servers_.insert(key, server);
        newInternetKeys.push_back(key);
      }

      internetKeys_ = newInternetKeys;
      loadFavoritesIntoState();
      populateServerTable();
      enqueuePingQueries(internetKeys_);
      statusBar()->showMessage(
          QString("Loaded %1 servers from open.mp API").arg(internetKeys_.size()));
    });
  }

  QStringList currentSourceKeys() const {
    switch (activeTab()) {
      case ActiveTab::Favorites: {
        QStringList keys;
        for (auto it = servers_.cbegin(); it != servers_.cend(); ++it) {
          if (it->favorite) {
            keys.push_back(it.key());
          }
        }
        std::sort(keys.begin(), keys.end(), [this](const QString &left, const QString &right) {
          return servers_.value(left).hostname.toLower() < servers_.value(right).hostname.toLower();
        });
        return keys;
      }
      case ActiveTab::Hosted: {
        QStringList keys;
        keys.reserve(internetKeys_.size());
        for (const auto &key : internetKeys_) {
          const auto it = servers_.constFind(key);
          if (it != servers_.cend() && it->partner) {
            keys.push_back(key);
          }
        }
        return keys;
      }
      case ActiveTab::Internet:
      default:
        return internetKeys_;
    }
  }

  void populateServerTable() {
    const QString previousSelection = currentSelectedKey();
    displayedKeys_.clear();
    const bool sortingWasEnabled = serverTable_->isSortingEnabled();
    const int sortSection = serverTable_->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = serverTable_->horizontalHeader()->sortIndicatorOrder();

    serverTable_->setSortingEnabled(false);

    const QString modeFilter = modeFilterEdit_->text().trimmed().toLower();
    const QString languageFilter = languageFilterEdit_->text().trimmed().toLower();
    const QString hostnameFilter = hostnameFilterEdit_->text().trimmed().toLower();

    for (const auto &key : currentSourceKeys()) {
      if (!servers_.contains(key)) {
        continue;
      }

      const auto &server = servers_[key];
      if (!modeFilter.isEmpty() && !server.mode.toLower().contains(modeFilter)) {
        continue;
      }
      if (!languageFilter.isEmpty() && !server.language.toLower().contains(languageFilter)) {
        continue;
      }
      if (!hostnameFilter.isEmpty() && !server.hostname.toLower().contains(hostnameFilter)) {
        continue;
      }
      if (notFullCheck_->isChecked() && server.maxPlayers > 0 && server.players >= server.maxPlayers) {
        continue;
      }
      if (notEmptyCheck_->isChecked() && server.players == 0) {
        continue;
      }
      if (noPasswordCheck_->isChecked() && server.locked) {
        continue;
      }
      displayedKeys_.push_back(key);
    }

    serverTable_->setRowCount(displayedKeys_.size());
    for (int row = 0; row < displayedKeys_.size(); ++row) {
      updateRow(row, servers_.value(displayedKeys_[row]));
    }

    serverTable_->setSortingEnabled(sortingWasEnabled);
    if (sortingWasEnabled) {
      serverTable_->sortItems(sortSection, sortOrder);
    }

    const int playersOnline =
        std::accumulate(displayedKeys_.cbegin(), displayedKeys_.cend(), 0, [this](int value, const QString &key) {
          return value + servers_.value(key).players;
        });
    statusBar()->showMessage(
        QString("Servers: %1 players, playing on %2 servers.").arg(playersOnline).arg(displayedKeys_.size()));

    int selectedRow = -1;
    if (!previousSelection.isEmpty()) {
      selectedRow = rowForKey(previousSelection);
    }
    if (selectedRow < 0 && serverTable_->rowCount() > 0) {
      selectedRow = 0;
    }
    if (selectedRow >= 0) {
      serverTable_->selectRow(selectedRow);
    } else {
      clearSelectionPanel();
    }
  }

  void updateRow(int row, const ServerData &server) {
    const QString key = serverKey(server.host, server.port);

    auto *favoriteItem = server.favorite
                             ? makeIconSortableItem(favoriteStateIcon(), "Favorite", 1)
                             : makeSortableItem(QString(), 0, Qt::AlignCenter);
    auto *lockItem = makeIconSortableItem(
        passwordStateIcon(server.locked), server.locked ? "Locked" : "Unlocked", server.locked ? 1 : 0);
    auto *hostItem = makeSortableItem(server.hostname, server.hostname.toLower());
    auto *playersItem = makeSortableItem(
        QString("%1 / %2").arg(server.players).arg(server.maxPlayers),
        static_cast<qint64>(server.players) * 100000 + server.maxPlayers, Qt::AlignCenter);
    auto *pingItem = makeSortableItem(
        displayPing(server.ping),
        server.ping < 0 || server.ping >= kPingTimeoutValue ? std::numeric_limits<qint64>::max() : server.ping,
        Qt::AlignCenter);
    auto *modeItem = makeSortableItem(server.mode, server.mode.toLower());
    auto *languageItem = makeSortableItem(server.language, server.language.toLower());

    for (auto *item : {static_cast<QTableWidgetItem *>(favoriteItem), static_cast<QTableWidgetItem *>(lockItem),
                       static_cast<QTableWidgetItem *>(hostItem), static_cast<QTableWidgetItem *>(playersItem),
                       static_cast<QTableWidgetItem *>(pingItem), static_cast<QTableWidgetItem *>(modeItem),
                       static_cast<QTableWidgetItem *>(languageItem)}) {
      item->setData(kServerKeyRole, key);
    }

    serverTable_->setItem(row, 0, favoriteItem);
    serverTable_->setItem(row, 1, lockItem);
    serverTable_->setItem(row, 2, hostItem);
    serverTable_->setItem(row, 3, playersItem);
    serverTable_->setItem(row, 4, pingItem);
    serverTable_->setItem(row, 5, modeItem);
    serverTable_->setItem(row, 6, languageItem);
  }

  QString currentSelectedKey() const {
    const auto selectedItems = serverTable_->selectedItems();
    if (selectedItems.isEmpty()) {
      return {};
    }
    return selectedItems.first()->data(kServerKeyRole).toString();
  }

  int rowForKey(const QString &key) const {
    for (int row = 0; row < serverTable_->rowCount(); ++row) {
      const auto *item = serverTable_->item(row, 0);
      if (item && item->data(kServerKeyRole).toString() == key) {
        return row;
      }
    }
    return -1;
  }

  void updateSelection() {
    const auto key = currentSelectedKey();
    if (key.isEmpty() || !servers_.contains(key)) {
      clearSelectionPanel();
      return;
    }

    activeDetailKey_ = key;
    detailRefreshTimer_.start();
    updateTabActions();
    updateSelectionPanel(servers_[key]);
    updateFavoriteActions(servers_[key]);
    startDetailQuery(key);
  }

  void updateSelectionPanel(const ServerData &server) {
    infoTitle_->setText(QString("Server Info: %1").arg(server.hostname));
    addressValue_->setText(serverKey(server.host, server.port));
    playersValue_->setText(QString("%1 / %2").arg(server.players).arg(server.maxPlayers));
    pingValue_->setText(server.ping < 0 ? "9999" : QString::number(server.ping));
    modeValue_->setText(server.mode.isEmpty() ? "-" : server.mode);
    languageValue_->setText(server.language.isEmpty() ? "-" : server.language);
    pingChart_->setSamples(server.pingSamples);

    playersTable_->setRowCount(server.playerEntries.size());
    for (int i = 0; i < server.playerEntries.size(); ++i) {
      playersTable_->setItem(i, 0, makeItem(server.playerEntries[i].name));
      playersTable_->setItem(i, 1, makeItem(QString::number(server.playerEntries[i].score), Qt::AlignCenter));
    }

    rulesTable_->setRowCount(server.rules.size());
    for (int i = 0; i < server.rules.size(); ++i) {
      rulesTable_->setItem(i, 0, makeItem(server.rules[i].first));
      rulesTable_->setItem(i, 1, makeItem(server.rules[i].second));
    }
  }

  void clearSelectionPanel() {
    activeDetailKey_.clear();
    detailRefreshTimer_.stop();
    infoTitle_->setText("Server Info");
    addressValue_->setText("-");
    playersValue_->setText("-");
    pingValue_->setText("-");
    modeValue_->setText("-");
    languageValue_->setText("-");
    pingChart_->setSamples({});
    playersTable_->setRowCount(0);
    rulesTable_->setRowCount(0);
    connectAction_->setEnabled(false);
    addFavoriteAction_->setEnabled(false);
    removeFavoriteAction_->setEnabled(false);
    serverPropertiesAction_->setEnabled(false);
    updateTabActions();
  }

  void updateTabActions() {
    if (addServerAction_ == nullptr) {
      return;
    }
    addServerAction_->setEnabled(tabs_ != nullptr && activeTab() == ActiveTab::Favorites);
  }

  void refreshSelectedServerDetails() {
    if (activeDetailKey_.isEmpty() || !servers_.contains(activeDetailKey_)) {
      detailRefreshTimer_.stop();
      return;
    }
    if (detailJob_) {
      return;
    }
    startDetailQuery(activeDetailKey_);
  }

  void updateFavoriteActions(const ServerData &server) {
    connectAction_->setEnabled(true);
    addFavoriteAction_->setEnabled(!server.favorite);
    removeFavoriteAction_->setEnabled(server.favorite);
    serverPropertiesAction_->setEnabled(true);
  }

  void addSelectedToFavorites() {
    const auto key = currentSelectedKey();
    if (key.isEmpty() || !servers_.contains(key)) {
      return;
    }

    auto &server = servers_[key];
    server.favorite = true;
    favoritesStore_.addOrUpdate(server);
    if (!favoritesStore_.save()) {
      favoritesStore_.remove(key);
      server.favorite = false;
      QMessageBox::warning(this, "Save Failed", "Could not save favorites.");
      return;
    }
    updateFavoriteActions(server);
    populateServerTable();
  }

  void removeSelectedFromFavorites() {
    const auto key = currentSelectedKey();
    if (key.isEmpty() || !servers_.contains(key)) {
      return;
    }

    auto &server = servers_[key];
    server.favorite = false;
    const auto previousRecord = favoritesStore_.recordForKey(key);
    favoritesStore_.remove(key);
    if (!favoritesStore_.save()) {
      favoritesStore_.addOrUpdate(previousRecord);
      server.favorite = true;
      QMessageBox::warning(this, "Save Failed", "Could not save favorites.");
      return;
    }
    updateFavoriteActions(server);
    populateServerTable();
  }

  void enqueuePingQueries(const QStringList &keys) {
    for (const auto &key : keys) {
      if (!servers_.contains(key) || pingJobs_.contains(key) || queuedPingKeys_.contains(key)) {
        continue;
      }
      pingQueue_.enqueue(key);
      queuedPingKeys_.insert(key);
    }
    dispatchPingQueries();
  }

  void dispatchPingQueries() {
    while (pingJobs_.size() < kMaxConcurrentPingJobs && !pingQueue_.isEmpty()) {
      const auto key = pingQueue_.dequeue();
      queuedPingKeys_.remove(key);
      if (!servers_.contains(key)) {
        continue;
      }

      const auto &server = servers_[key];
      auto *job = new SampQueryJob(server.host, server.port, QueryOptions{false, false, false, false, true}, this);
      pingJobs_.insert(key, job);
      job->setFinishedCallback([this, key, job]() {
        applyQueryResult(key, job->result());
        pingJobs_.remove(key);
        job->deleteLater();
        dispatchPingQueries();
      });
      job->start();
    }
  }

  void startDetailQuery(const QString &key) {
    if (!servers_.contains(key)) {
      return;
    }

    activeDetailKey_ = key;
    if (detailJob_) {
      detailJob_->setFinishedCallback({});
      detailJob_->deleteLater();
      detailJob_.clear();
    }

    const auto &server = servers_[key];
    detailJob_ =
        new SampQueryJob(server.host, server.port, QueryOptions{true, true, true, false, true}, this);
    auto *job = detailJob_.data();
    detailJob_->setFinishedCallback([this, key, job]() {
      if (detailJob_ != job) {
        return;
      }
      applyQueryResult(key, job->result());
      job->deleteLater();
      detailJob_.clear();
    });
    detailJob_->start();
  }

  void applyQueryResult(const QString &key, const QueryResult &result) {
    auto it = servers_.find(key);
    if (it == servers_.end()) {
      return;
    }

    auto &server = it.value();
    if (result.infoReceived) {
      server.locked = result.locked;
      server.players = result.players;
      server.maxPlayers = result.maxPlayers;
      if (!result.hostname.isEmpty()) {
        server.hostname = result.hostname;
        if (server.favorite) {
          FavoriteRecord updated = favoritesStore_.recordForKey(key);
          updated.name = result.hostname;
          updated.host = server.host;
          updated.port = server.port;
          favoritesStore_.addOrUpdate(updated);
          favoritesStore_.save();
        }
      }
      server.mode = result.gamemode;
      server.language = result.language;
    }
    if (result.playersReceived) {
      server.playerEntries = result.playerEntries;
    }
    if (result.rulesReceived) {
      server.rules = result.rules;
      for (const auto &rule : server.rules) {
        if (rule.first == "version") {
          server.version = rule.second;
        }
      }
      server.usingOmp = isUsingOmp(server.rules, server.version);
    }
    if (result.pingReceived) {
      server.ping = result.ping;
      if (result.ping >= 0 && result.ping < kPingTimeoutValue) {
        server.pingSamples.push_back(result.ping);
        if (server.pingSamples.size() > kMaxPingSamples) {
          server.pingSamples.remove(0, server.pingSamples.size() - kMaxPingSamples);
        }
      }
    }

    server.lastQueryAt = QDateTime::currentDateTimeUtc();

    const int row = rowForKey(key);
    if (row >= 0) {
      const bool sortingWasEnabled = serverTable_->isSortingEnabled();
      serverTable_->setSortingEnabled(false);
      updateRow(row, server);
      serverTable_->setSortingEnabled(sortingWasEnabled);
      if (sortingWasEnabled) {
        serverTable_->sortItems(serverTable_->horizontalHeader()->sortIndicatorSection(),
                                serverTable_->horizontalHeader()->sortIndicatorOrder());
      }
    }
    if (key == currentSelectedKey()) {
      updateSelectionPanel(server);
      updateFavoriteActions(server);
    }
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QApplication::setStyle("Fusion");
  app.setApplicationName("omp-launcher-classic");
  app.setApplicationDisplayName("open.mp Classic");
  app.setOrganizationName("openmp");
  app.setDesktopFileName("omp-launcher-classic");

#ifdef Q_OS_WIN
  SetCurrentProcessExplicitAppUserModelID(L"open.mp.classic");
#endif

  app.setWindowIcon(launcherAppIcon());

  LauncherWindow window;
  window.show();
  return app.exec();
}
