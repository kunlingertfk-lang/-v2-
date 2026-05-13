#ifndef TOOLCORE_TOOLREQUEST_H
#define TOOLCORE_TOOLREQUEST_H

#include "ToolConfig.h"

#include <QJsonObject>
#include <QString>
#include <opencv2/core.hpp>

struct ToolRequest {
    int schemaVersion = 1;
    QString requestId;
    ToolConfig config;
    cv::Mat image;
    cv::Mat referenceImage;
    QString imagePath;
    QString imageFormat;
    QString frameId;
    QString sharedMemoryKey;
    QJsonObject runtimeContext;

    ToolRequest() = default;

    QJsonObject toJson() const
    {
        QJsonObject imageRef;
        imageRef.insert(QStringLiteral("path"), imagePath);
        imageRef.insert(QStringLiteral("format"), imageFormat);
        imageRef.insert(QStringLiteral("frameId"), frameId);
        imageRef.insert(QStringLiteral("sharedMemoryKey"), sharedMemoryKey);
        imageRef.insert(QStringLiteral("hasMat"), !image.empty());
        imageRef.insert(QStringLiteral("width"), image.empty() ? 0 : image.cols);
        imageRef.insert(QStringLiteral("height"), image.empty() ? 0 : image.rows);
        imageRef.insert(QStringLiteral("channels"), image.empty() ? 0 : image.channels());
        imageRef.insert(QStringLiteral("matType"), image.empty() ? -1 : image.type());

        QJsonObject referenceImageRef;
        referenceImageRef.insert(QStringLiteral("hasMat"), !referenceImage.empty());
        referenceImageRef.insert(QStringLiteral("width"), referenceImage.empty() ? 0 : referenceImage.cols);
        referenceImageRef.insert(QStringLiteral("height"), referenceImage.empty() ? 0 : referenceImage.rows);
        referenceImageRef.insert(QStringLiteral("channels"), referenceImage.empty() ? 0 : referenceImage.channels());
        referenceImageRef.insert(QStringLiteral("matType"), referenceImage.empty() ? -1 : referenceImage.type());

        QJsonObject json;
        json.insert(QStringLiteral("schemaVersion"), schemaVersion);
        json.insert(QStringLiteral("requestId"), requestId);
        json.insert(QStringLiteral("config"), config.toJson());
        json.insert(QStringLiteral("imageRef"), imageRef);
        json.insert(QStringLiteral("referenceImageRef"), referenceImageRef);
        json.insert(QStringLiteral("runtimeContext"), runtimeContext);
        return json;
    }

    static ToolRequest fromJson(const QJsonObject &json)
    {
        ToolRequest request;
        request.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
        request.requestId = json.value(QStringLiteral("requestId")).toString();
        request.config = ToolConfig::fromJson(json.value(QStringLiteral("config")).toObject());

        const QJsonObject imageRef = json.value(QStringLiteral("imageRef")).toObject();
        request.imagePath = imageRef.value(QStringLiteral("path")).toString();
        request.imageFormat = imageRef.value(QStringLiteral("format")).toString();
        request.frameId = imageRef.value(QStringLiteral("frameId")).toString();
        request.sharedMemoryKey = imageRef.value(QStringLiteral("sharedMemoryKey")).toString();
        request.runtimeContext = json.value(QStringLiteral("runtimeContext")).toObject();
        return request;
    }
};

#endif // TOOLCORE_TOOLREQUEST_H
