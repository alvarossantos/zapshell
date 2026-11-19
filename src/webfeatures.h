#pragma once

// Espelho de QWebEnginePermission::PermissionType usado pela política do app.
// web.cpp tem static_assert contra os valores reais, então uma divergência
// quebra a compilação em vez de negar permissão silenciosamente.
//
// Qt 6.12 deprecou QWebEnginePage::Feature e seus valores NÃO são bit flags.
namespace zs {

constexpr int FeatureMediaAudioCapture = 1;
constexpr int FeatureMediaVideoCapture = 2;
constexpr int FeatureMediaAudioVideoCapture = 3;
constexpr int FeatureDesktopVideoCapture = 4;
constexpr int FeatureDesktopAudioVideoCapture = 5;
constexpr int FeatureNotifications = 7;

} // namespace zs
