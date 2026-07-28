#ifndef FRAME_ROIEDITORCONTROLLER_H
#define FRAME_ROIEDITORCONTROLLER_H

#include <QPointF>
#include <QRectF>
#include <QLineF>
#include <QtGlobal>

#include <cmath>

// ROI 变换的纯几何控制层。它不依赖 QGraphicsScene，也不保存业务配置；
// FrameViewHelper 负责坐标转换和绘制，各工具继续接收原有 roiChanged 信号。
class RoiEditorController
{
public:
    enum class Handle {
        None,
        Move,
        Left,
        Top,
        Right,
        Bottom,
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft
    };

    enum class CircleHandle {
        None,
        Move,
        Radius
    };

    enum class LineBandHandle {
        None,
        Move,
        FirstEndpoint,
        SecondEndpoint,
        Width
    };

    struct CircleGeometry {
        QPointF center;
        qreal radius = 0.0;
    };

    struct LineBandGeometry {
        QPointF p1;
        QPointF p2;
        qreal width = 0.0;
    };

    void setBounds(const QRectF &bounds)
    {
        m_bounds = bounds.normalized();
    }

    void setMinimumSize(qreal minimumSize)
    {
        m_minimumSize = qMax<qreal>(0.0, minimumSize);
    }

    Handle hitTest(const QRectF &rect, const QPointF &point, qreal tolerance) const
    {
        if (!isUsableRect(rect) || !finitePoint(point))
            return Handle::None;

        const QRectF normalized = rect.normalized();
        const qreal radius = qMax<qreal>(0.0, tolerance);
        const QPointF topLeft = normalized.topLeft();
        const QPointF topRight = normalized.topRight();
        const QPointF bottomRight = normalized.bottomRight();
        const QPointF bottomLeft = normalized.bottomLeft();
        const QPointF top(normalized.center().x(), normalized.top());
        const QPointF right(normalized.right(), normalized.center().y());
        const QPointF bottom(normalized.center().x(), normalized.bottom());
        const QPointF left(normalized.left(), normalized.center().y());

        if (near(point, topLeft, radius))
            return Handle::TopLeft;
        if (near(point, topRight, radius))
            return Handle::TopRight;
        if (near(point, bottomRight, radius))
            return Handle::BottomRight;
        if (near(point, bottomLeft, radius))
            return Handle::BottomLeft;
        if (near(point, top, radius))
            return Handle::Top;
        if (near(point, right, radius))
            return Handle::Right;
        if (near(point, bottom, radius))
            return Handle::Bottom;
        if (near(point, left, radius))
            return Handle::Left;
        if (normalized.contains(point))
            return Handle::Move;
        return Handle::None;
    }

    bool isNearRectangle(const QRectF &rect,
                         const QPointF &point,
                         qreal margin) const
    {
        if (!isUsableRect(rect) || !finitePoint(point))
            return false;
        return rect.normalized().adjusted(-margin, -margin, margin, margin)
                .contains(point);
    }

    bool beginRectangle(const QRectF &rect, const QPointF &point, qreal tolerance)
    {
        const Handle handle = hitTest(rect, point, tolerance);
        if (handle == Handle::None)
            return false;

        m_startRect = rect.normalized();
        m_currentRect = m_startRect;
        m_startPoint = point;
        m_handle = handle;
        m_active = true;
        return true;
    }

    QRectF updateRectangle(const QPointF &point)
    {
        if (!m_active || !finitePoint(point))
            return m_currentRect;

        if (m_handle == Handle::Move) {
            const QPointF requestedDelta = point - m_startPoint;
            const qreal minimumDx = m_bounds.left() - m_startRect.left();
            const qreal maximumDx = m_bounds.right() - m_startRect.right();
            const qreal minimumDy = m_bounds.top() - m_startRect.top();
            const qreal maximumDy = m_bounds.bottom() - m_startRect.bottom();
            const QPointF delta(qBound(minimumDx, requestedDelta.x(), maximumDx),
                                qBound(minimumDy, requestedDelta.y(), maximumDy));
            m_currentRect = m_startRect.translated(delta);
            return m_currentRect;
        }

        qreal left = m_startRect.left();
        qreal top = m_startRect.top();
        qreal right = m_startRect.right();
        qreal bottom = m_startRect.bottom();

        if (movesLeft(m_handle))
            left = qBound(m_bounds.left(), point.x(), right - m_minimumSize);
        if (movesRight(m_handle))
            right = qBound(left + m_minimumSize, point.x(), m_bounds.right());
        if (movesTop(m_handle))
            top = qBound(m_bounds.top(), point.y(), bottom - m_minimumSize);
        if (movesBottom(m_handle))
            bottom = qBound(top + m_minimumSize, point.y(), m_bounds.bottom());

        m_currentRect = QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
        return m_currentRect;
    }

    QRectF finishRectangle()
    {
        m_active = false;
        m_handle = Handle::None;
        return m_currentRect;
    }

    QRectF cancel()
    {
        m_active = false;
        m_handle = Handle::None;
        m_currentRect = m_startRect;
        return m_startRect;
    }

    bool isActive() const
    {
        return m_active;
    }

    Handle activeHandle() const
    {
        return m_handle;
    }

    CircleHandle hitTestCircle(const CircleGeometry &circle,
                               const QPointF &point,
                               qreal tolerance) const
    {
        if (!finitePoint(circle.center) || !finiteValue(circle.radius)
                || circle.radius <= 0.0 || !finitePoint(point)) {
            return CircleHandle::None;
        }

        const QPointF radiusHandle(circle.center.x() + circle.radius,
                                   circle.center.y());
        if (near(point, radiusHandle, qMax<qreal>(0.0, tolerance)))
            return CircleHandle::Radius;
        if (QLineF(circle.center, point).length() <= circle.radius)
            return CircleHandle::Move;
        return CircleHandle::None;
    }

    bool isNearCircle(const CircleGeometry &circle,
                      const QPointF &point,
                      qreal margin) const
    {
        if (!finitePoint(circle.center) || !finiteValue(circle.radius)
                || circle.radius <= 0.0 || !finitePoint(point)) {
            return false;
        }
        return QLineF(circle.center, point).length()
                <= circle.radius + qMax<qreal>(0.0, margin);
    }

    bool beginCircle(const CircleGeometry &circle,
                     const QPointF &point,
                     qreal tolerance)
    {
        const CircleHandle handle = hitTestCircle(circle, point, tolerance);
        if (handle == CircleHandle::None)
            return false;

        m_startCircle = circle;
        m_currentCircle = circle;
        m_startPoint = point;
        m_circleHandle = handle;
        m_circleActive = true;
        return true;
    }

    CircleGeometry updateCircle(const QPointF &point)
    {
        if (!m_circleActive || !finitePoint(point))
            return m_currentCircle;

        if (m_circleHandle == CircleHandle::Move) {
            const QPointF requestedDelta = point - m_startPoint;
            const qreal minimumDx = m_bounds.left()
                    - (m_startCircle.center.x() - m_startCircle.radius);
            const qreal maximumDx = m_bounds.right()
                    - (m_startCircle.center.x() + m_startCircle.radius);
            const qreal minimumDy = m_bounds.top()
                    - (m_startCircle.center.y() - m_startCircle.radius);
            const qreal maximumDy = m_bounds.bottom()
                    - (m_startCircle.center.y() + m_startCircle.radius);
            const QPointF delta(qBound(minimumDx, requestedDelta.x(), maximumDx),
                                qBound(minimumDy, requestedDelta.y(), maximumDy));
            m_currentCircle.center = m_startCircle.center + delta;
            return m_currentCircle;
        }

        if (m_circleHandle == CircleHandle::Radius) {
            const qreal maximumRadius = qMax<qreal>(m_minimumSize,
                    qMin(qMin(m_startCircle.center.x() - m_bounds.left(),
                              m_bounds.right() - m_startCircle.center.x()),
                         qMin(m_startCircle.center.y() - m_bounds.top(),
                              m_bounds.bottom() - m_startCircle.center.y())));
            m_currentCircle.radius = qBound(m_minimumSize,
                    static_cast<qreal>(QLineF(m_startCircle.center, point).length()),
                    maximumRadius);
        }
        return m_currentCircle;
    }

    CircleGeometry finishCircle()
    {
        m_circleActive = false;
        m_circleHandle = CircleHandle::None;
        return m_currentCircle;
    }

    CircleGeometry cancelCircle()
    {
        m_circleActive = false;
        m_circleHandle = CircleHandle::None;
        m_currentCircle = m_startCircle;
        return m_startCircle;
    }

    bool isCircleActive() const
    {
        return m_circleActive;
    }

    CircleHandle activeCircleHandle() const
    {
        return m_circleHandle;
    }

    LineBandHandle hitTestLineBand(const LineBandGeometry &lineBand,
                                   const QPointF &point,
                                   qreal tolerance) const
    {
        if (!isUsableLineBand(lineBand) || !finitePoint(point))
            return LineBandHandle::None;

        const qreal radius = qMax<qreal>(0.0, tolerance);
        if (near(point, lineBand.p1, radius))
            return LineBandHandle::FirstEndpoint;
        if (near(point, lineBand.p2, radius))
            return LineBandHandle::SecondEndpoint;
        if (near(point, lineBandWidthHandle(lineBand), radius))
            return LineBandHandle::Width;
        if (distanceToSegment(point, lineBand.p1, lineBand.p2)
                <= lineBand.width / 2.0 + radius) {
            return LineBandHandle::Move;
        }
        return LineBandHandle::None;
    }

    bool isNearLineBand(const LineBandGeometry &lineBand,
                        const QPointF &point,
                        qreal margin) const
    {
        if (!isUsableLineBand(lineBand) || !finitePoint(point))
            return false;
        return distanceToSegment(point, lineBand.p1, lineBand.p2)
                <= lineBand.width / 2.0 + qMax<qreal>(0.0, margin);
    }

    bool beginLineBand(const LineBandGeometry &lineBand,
                       const QPointF &point,
                       qreal tolerance)
    {
        const LineBandHandle handle = hitTestLineBand(lineBand, point, tolerance);
        if (handle == LineBandHandle::None)
            return false;

        m_startLineBand = lineBand;
        m_currentLineBand = lineBand;
        m_startPoint = point;
        m_lineBandHandle = handle;
        m_lineBandActive = true;
        return true;
    }

    LineBandGeometry updateLineBand(const QPointF &point)
    {
        if (!m_lineBandActive || !finitePoint(point))
            return m_currentLineBand;

        if (m_lineBandHandle == LineBandHandle::Move) {
            const QPointF requestedDelta = point - m_startPoint;
            const QRectF startBounds = lineBandBounds(m_startLineBand);
            const qreal minimumDx = m_bounds.left() - startBounds.left();
            const qreal maximumDx = m_bounds.right() - startBounds.right();
            const qreal minimumDy = m_bounds.top() - startBounds.top();
            const qreal maximumDy = m_bounds.bottom() - startBounds.bottom();
            const QPointF delta(qBound(minimumDx, requestedDelta.x(), maximumDx),
                                qBound(minimumDy, requestedDelta.y(), maximumDy));
            m_currentLineBand.p1 = m_startLineBand.p1 + delta;
            m_currentLineBand.p2 = m_startLineBand.p2 + delta;
            return m_currentLineBand;
        }

        const QPointF bounded(qBound(m_bounds.left(), point.x(), m_bounds.right()),
                              qBound(m_bounds.top(), point.y(), m_bounds.bottom()));
        if (m_lineBandHandle == LineBandHandle::FirstEndpoint) {
            if (QLineF(bounded, m_startLineBand.p2).length() >= m_minimumSize)
                m_currentLineBand.p1 = bounded;
        } else if (m_lineBandHandle == LineBandHandle::SecondEndpoint) {
            if (QLineF(m_startLineBand.p1, bounded).length() >= m_minimumSize)
                m_currentLineBand.p2 = bounded;
        } else if (m_lineBandHandle == LineBandHandle::Width) {
            const qreal distance = distanceToInfiniteLine(
                        point, m_startLineBand.p1, m_startLineBand.p2);
            m_currentLineBand.width = qBound(m_minimumSize,
                    distance * 2.0,
                    qMax(m_bounds.width(), m_bounds.height()));
        }
        return m_currentLineBand;
    }

    LineBandGeometry finishLineBand()
    {
        m_lineBandActive = false;
        m_lineBandHandle = LineBandHandle::None;
        return m_currentLineBand;
    }

    LineBandGeometry cancelLineBand()
    {
        m_lineBandActive = false;
        m_lineBandHandle = LineBandHandle::None;
        m_currentLineBand = m_startLineBand;
        return m_startLineBand;
    }

    bool isLineBandActive() const
    {
        return m_lineBandActive;
    }

    LineBandHandle activeLineBandHandle() const
    {
        return m_lineBandHandle;
    }

    static QPointF lineBandWidthHandle(const LineBandGeometry &lineBand)
    {
        const QLineF line(lineBand.p1, lineBand.p2);
        if (line.length() <= 0.0)
            return lineBand.p1;
        const QPointF midpoint = (lineBand.p1 + lineBand.p2) / 2.0;
        const QPointF normal(-(lineBand.p2.y() - lineBand.p1.y()) / line.length(),
                             (lineBand.p2.x() - lineBand.p1.x()) / line.length());
        return midpoint + normal * (lineBand.width / 2.0);
    }

private:
    static bool finiteValue(qreal value)
    {
        return std::isfinite(static_cast<double>(value));
    }

    static bool finitePoint(const QPointF &point)
    {
        return std::isfinite(static_cast<double>(point.x()))
                && std::isfinite(static_cast<double>(point.y()));
    }

    static bool isUsableRect(const QRectF &rect)
    {
        return finitePoint(rect.topLeft()) && finitePoint(rect.bottomRight())
                && rect.width() > 0.0 && rect.height() > 0.0;
    }

    static bool near(const QPointF &left, const QPointF &right, qreal tolerance)
    {
        const QPointF delta = left - right;
        return delta.x() * delta.x() + delta.y() * delta.y()
                <= tolerance * tolerance;
    }

    static bool movesLeft(Handle handle)
    {
        return handle == Handle::Left || handle == Handle::TopLeft
                || handle == Handle::BottomLeft;
    }

    static bool movesRight(Handle handle)
    {
        return handle == Handle::Right || handle == Handle::TopRight
                || handle == Handle::BottomRight;
    }

    static bool movesTop(Handle handle)
    {
        return handle == Handle::Top || handle == Handle::TopLeft
                || handle == Handle::TopRight;
    }

    static bool movesBottom(Handle handle)
    {
        return handle == Handle::Bottom || handle == Handle::BottomLeft
                || handle == Handle::BottomRight;
    }

    static bool isUsableLineBand(const LineBandGeometry &lineBand)
    {
        return finitePoint(lineBand.p1) && finitePoint(lineBand.p2)
                && finiteValue(lineBand.width) && lineBand.width > 0.0
                && QLineF(lineBand.p1, lineBand.p2).length() > 0.0;
    }

    static qreal distanceToInfiniteLine(const QPointF &point,
                                        const QPointF &p1,
                                        const QPointF &p2)
    {
        const qreal length = QLineF(p1, p2).length();
        if (length <= 0.0)
            return 0.0;
        return std::abs((point.x() - p1.x()) * (p2.y() - p1.y())
                        - (point.y() - p1.y()) * (p2.x() - p1.x())) / length;
    }

    static qreal distanceToSegment(const QPointF &point,
                                   const QPointF &p1,
                                   const QPointF &p2)
    {
        const QPointF delta = p2 - p1;
        const qreal lengthSquared = delta.x() * delta.x() + delta.y() * delta.y();
        if (lengthSquared <= 0.0)
            return QLineF(point, p1).length();
        const QPointF relative = point - p1;
        const qreal projection = qBound<qreal>(0.0,
                (relative.x() * delta.x() + relative.y() * delta.y())
                / lengthSquared,
                1.0);
        return QLineF(point, p1 + delta * projection).length();
    }

    static QRectF lineBandBounds(const LineBandGeometry &lineBand)
    {
        const QLineF line(lineBand.p1, lineBand.p2);
        if (line.length() <= 0.0)
            return QRectF(lineBand.p1, QSizeF());
        const QPointF normal(-(lineBand.p2.y() - lineBand.p1.y()) / line.length(),
                             (lineBand.p2.x() - lineBand.p1.x()) / line.length());
        const QPointF offset = normal * (lineBand.width / 2.0);
        const qreal left = qMin(qMin(lineBand.p1.x() + offset.x(),
                                    lineBand.p1.x() - offset.x()),
                                qMin(lineBand.p2.x() + offset.x(),
                                    lineBand.p2.x() - offset.x()));
        const qreal right = qMax(qMax(lineBand.p1.x() + offset.x(),
                                     lineBand.p1.x() - offset.x()),
                                 qMax(lineBand.p2.x() + offset.x(),
                                     lineBand.p2.x() - offset.x()));
        const qreal top = qMin(qMin(lineBand.p1.y() + offset.y(),
                                   lineBand.p1.y() - offset.y()),
                               qMin(lineBand.p2.y() + offset.y(),
                                   lineBand.p2.y() - offset.y()));
        const qreal bottom = qMax(qMax(lineBand.p1.y() + offset.y(),
                                      lineBand.p1.y() - offset.y()),
                                  qMax(lineBand.p2.y() + offset.y(),
                                      lineBand.p2.y() - offset.y()));
        return QRectF(QPointF(left, top), QPointF(right, bottom));
    }

    QRectF m_bounds;
    QRectF m_startRect;
    QRectF m_currentRect;
    QPointF m_startPoint;
    qreal m_minimumSize = 2.0;
    Handle m_handle = Handle::None;
    bool m_active = false;
    CircleGeometry m_startCircle;
    CircleGeometry m_currentCircle;
    CircleHandle m_circleHandle = CircleHandle::None;
    bool m_circleActive = false;
    LineBandGeometry m_startLineBand;
    LineBandGeometry m_currentLineBand;
    LineBandHandle m_lineBandHandle = LineBandHandle::None;
    bool m_lineBandActive = false;
};

#endif // FRAME_ROIEDITORCONTROLLER_H
