#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D staticTransmission;
uniform sampler2D staticOccupancy;
uniform sampler2D dynamicTransmission;
uniform sampler2D dynamicOccupancy;
uniform sampler2D cachedStaticLight;

uniform vec2 targetPixelScale;
uniform vec2 screenSize;
uniform vec2 mapViewOffset;
uniform vec2 viewPos;
uniform vec2 viewSinCos;
uniform vec2 gridSize;
uniform float cellSize;

uniform vec2 dynamicMaskOrigin;
uniform vec2 dynamicMaskSize;
uniform vec2 dynamicOccupancyOrigin;
uniform vec2 dynamicOccupancySize;
uniform float useCachedStaticLight;

uniform vec2 lightPos;
uniform vec3 lightColor;
uniform float lightRadius;
uniform float lightIntensity;
uniform float traceStatic;
uniform float traceDynamic;
uniform float staticViewMode;
uniform vec2 staticTextureOrigin;
uniform vec2 staticTextureSize;
uniform vec2 staticOccupancyOrigin;
uniform vec2 staticOccupancySize;


vec2 rotate2D(vec2 v, vec2 sinCos) {
    return vec2(
        v.x * sinCos.y - v.y * sinCos.x,
        v.x * sinCos.x + v.y * sinCos.y
    );
}

float SampleStaticTransmission(vec2 worldPosTL) {
    if (staticViewMode > 0.5) {
        vec2 viewUV = (worldPosTL - staticTextureOrigin) / staticTextureSize;
        if (
            viewUV.x < 0.0 ||
            viewUV.y < 0.0 ||
            viewUV.x >= 1.0 ||
            viewUV.y >= 1.0
        ) {
            return 1.0;
        }
        return clamp(
            texture2D(staticTransmission, vec2(viewUV.x, 1.0 - viewUV.y)).r,
            0.0,
            1.0
        );
    }
    vec2 worldSize = gridSize * cellSize;
    vec2 uv = worldPosTL / worldSize;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
        return 1.0;
    }
    return clamp(
        texture2D(staticTransmission, vec2(uv.x, 1.0 - uv.y)).r,
        0.0,
        1.0
    );
}

float SampleStaticOccupancy(vec2 cell) {
    if (staticViewMode > 0.5) {
        vec2 localCell = cell - staticOccupancyOrigin;
        if (
            localCell.x < 0.0 ||
            localCell.y < 0.0 ||
            localCell.x >= staticOccupancySize.x ||
            localCell.y >= staticOccupancySize.y
        ) {
            return 0.0;
        }
        vec2 viewUV = (localCell + vec2(0.5)) / staticOccupancySize;
        return texture2D(
            staticOccupancy,
            vec2(viewUV.x, 1.0 - viewUV.y)
        ).r;
    }
    if (
        cell.x < 0.0 ||
        cell.y < 0.0 ||
        cell.x >= gridSize.x ||
        cell.y >= gridSize.y
    ) {
        return 0.0;
    }
    vec2 uv = (cell + vec2(0.5)) / gridSize;
    return texture2D(
        staticOccupancy,
        vec2(uv.x, 1.0 - uv.y)
    ).r;
}

float SampleDynamicTransmission(vec2 worldPosTL) {
    if (dynamicMaskSize.x <= 0.0 || dynamicMaskSize.y <= 0.0) {
        return 1.0;
    }

    vec2 uv = (worldPosTL - dynamicMaskOrigin) / dynamicMaskSize;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
        return 1.0;
    }
    return clamp(
        texture2D(dynamicTransmission, vec2(uv.x, 1.0 - uv.y)).r,
        0.0,
        1.0
    );
}

float SampleDynamicOccupancy(vec2 cell) {
    vec2 localCell = cell - dynamicOccupancyOrigin;
    if (
        localCell.x < 0.0 ||
        localCell.y < 0.0 ||
        localCell.x >= dynamicOccupancySize.x ||
        localCell.y >= dynamicOccupancySize.y
    ) {
        return 0.0;
    }
    vec2 uv = (localCell + vec2(0.5)) / dynamicOccupancySize;
    return texture2D(dynamicOccupancy, vec2(uv.x, 1.0 - uv.y)).r;
}

bool AccumulateOpticalDepth(
    float transmission,
    float segmentLength,
    inout float opticalDepth
) {
    if (transmission <= 0.000001) {
        return false;
    }
    if (transmission < 0.999999) {
        opticalDepth +=
            -log(max(transmission, 0.000001)) *
            segmentLength /
            cellSize;
    }
    return true;
}

float TraceDynamicFineRange(
    vec2 start,
    vec2 direction,
    float rayLength
) {
    int sampleCount = int(ceil(rayLength));
    if (sampleCount < 1) {
        sampleCount = 1;
    }
    if (sampleCount > 12) {
        sampleCount = 12;
    }
    float sampleLength = rayLength / float(sampleCount);
    float opticalDepth = 0.0;
    for (int i = 0; i < 12; ++i) {
        if (i >= sampleCount) {
            break;
        }
        float distance = (float(i) + 0.5) * sampleLength;
        float transmission = SampleDynamicTransmission(
            start + direction * distance
        );
        if (!AccumulateOpticalDepth(
            transmission,
            sampleLength,
            opticalDepth
        )) {
            return 0.0;
        }
    }
    return opticalDepth > 0.0 ? exp(-opticalDepth) : 1.0;
}

float TraceDynamicHierarchy(
    vec2 start,
    vec2 direction,
    float rayLength
) {
    vec2 cell = floor(
        (start + direction * 0.0001) /
        cellSize
    );
    vec2 stepDirection = sign(direction);
    vec2 nextBoundary =
        (cell + max(stepDirection, vec2(0.0))) *
        cellSize;
    vec2 nextDistance = vec2(1.0e20);
    vec2 distanceDelta = vec2(1.0e20);

    if (direction.x != 0.0) {
        nextDistance.x = (nextBoundary.x - start.x) / direction.x;
        distanceDelta.x = cellSize / abs(direction.x);
    }
    if (direction.y != 0.0) {
        nextDistance.y = (nextBoundary.y - start.y) / direction.y;
        distanceDelta.y = cellSize / abs(direction.y);
    }

    float transmission = 1.0;
    float travelled = 0.0;
    for (int i = 0; i < 1024; ++i) {
        float segmentEnd =
            min(min(nextDistance.x, nextDistance.y), rayLength);
        float segmentLength = max(segmentEnd - travelled, 0.0);
        if (
            segmentLength > 0.0 &&
            SampleDynamicOccupancy(cell) > 0.0
        ) {
            float segmentTransmission = TraceDynamicFineRange(
                start + direction * travelled,
                direction,
                segmentLength
            );
            if (segmentTransmission <= 0.000001) {
                return 0.0;
            }
            transmission *= segmentTransmission;
        }

        travelled = segmentEnd;
        if (travelled >= rayLength - 0.000001) {
            return transmission;
        }
        if (nextDistance.x <= segmentEnd + 0.000001) {
            cell.x += stepDirection.x;
            nextDistance.x += distanceDelta.x;
        }
        if (nextDistance.y <= segmentEnd + 0.000001) {
            cell.y += stepDirection.y;
            nextDistance.y += distanceDelta.y;
        }
    }
    return 0.0;
}

bool ClipDynamicAxis(
    float start,
    float direction,
    float minimumValue,
    float maximumValue,
    inout float rangeStart,
    inout float rangeEnd
) {
    if (abs(direction) <= 0.000001) {
        return start >= minimumValue && start < maximumValue;
    }
    float first = (minimumValue - start) / direction;
    float second = (maximumValue - start) / direction;
    if (first > second) {
        float swapValue = first;
        first = second;
        second = swapValue;
    }
    rangeStart = max(rangeStart, first);
    rangeEnd = min(rangeEnd, second);
    return rangeEnd > rangeStart;
}

float TraceDynamicMask(
    vec2 start,
    vec2 direction,
    float rayLength
) {
    if (dynamicMaskSize.x <= 0.0 || dynamicMaskSize.y <= 0.0) {
        return 1.0;
    }
    float rangeStart = 0.0;
    float rangeEnd = rayLength;
    vec2 maskEnd = dynamicMaskOrigin + dynamicMaskSize;
    if (!ClipDynamicAxis(
        start.x,
        direction.x,
        dynamicMaskOrigin.x,
        maskEnd.x,
        rangeStart,
        rangeEnd
    )) {
        return 1.0;
    }
    if (!ClipDynamicAxis(
        start.y,
        direction.y,
        dynamicMaskOrigin.y,
        maskEnd.y,
        rangeStart,
        rangeEnd
    )) {
        return 1.0;
    }
    rangeStart = clamp(rangeStart, 0.0, rayLength);
    rangeEnd = clamp(rangeEnd, 0.0, rayLength);
    if (rangeEnd <= rangeStart) {
        return 1.0;
    }
    return TraceDynamicHierarchy(
        start + direction * rangeStart,
        direction,
        rangeEnd - rangeStart
    );
}

float TraceStaticFineSegment(
    vec2 start,
    vec2 direction,
    float rayLength
) {
    vec2 cell = floor(start + direction * 0.0001);
    vec2 stepDirection = sign(direction);
    vec2 nextBoundary = cell + max(stepDirection, vec2(0.0));
    vec2 nextDistance = vec2(1.0e20);
    vec2 distanceDelta = vec2(1.0e20);

    if (direction.x != 0.0) {
        nextDistance.x = (nextBoundary.x - start.x) / direction.x;
        distanceDelta.x = 1.0 / abs(direction.x);
    }
    if (direction.y != 0.0) {
        nextDistance.y = (nextBoundary.y - start.y) / direction.y;
        distanceDelta.y = 1.0 / abs(direction.y);
    }

    float opticalDepth = 0.0;
    float travelled = 0.0;
    for (int i = 0; i < 256; ++i) {
        float segmentEnd =
            min(min(nextDistance.x, nextDistance.y), rayLength);
        float segmentLength = max(segmentEnd - travelled, 0.0);
        if (segmentLength > 0.0) {
            float transmission =
                SampleStaticTransmission(cell + vec2(0.5));
            if (!AccumulateOpticalDepth(
                transmission,
                segmentLength,
                opticalDepth
            )) {
                return 0.0;
            }
        }

        travelled = segmentEnd;
        if (travelled >= rayLength - 0.000001) {
            return opticalDepth > 0.0 ? exp(-opticalDepth) : 1.0;
        }
        if (nextDistance.x <= segmentEnd + 0.000001) {
            cell.x += stepDirection.x;
            nextDistance.x += distanceDelta.x;
        }
        if (nextDistance.y <= segmentEnd + 0.000001) {
            cell.y += stepDirection.y;
            nextDistance.y += distanceDelta.y;
        }
    }
    return 0.0;
}

float TraceStaticHierarchy(
    vec2 start,
    vec2 direction,
    float rayLength
) {
    vec2 cell = floor(
        (start + direction * 0.0001) /
        cellSize
    );
    vec2 stepDirection = sign(direction);
    vec2 nextBoundary =
        (cell + max(stepDirection, vec2(0.0))) *
        cellSize;
    vec2 nextDistance = vec2(1.0e20);
    vec2 distanceDelta = vec2(1.0e20);

    if (direction.x != 0.0) {
        nextDistance.x = (nextBoundary.x - start.x) / direction.x;
        distanceDelta.x = cellSize / abs(direction.x);
    }
    if (direction.y != 0.0) {
        nextDistance.y = (nextBoundary.y - start.y) / direction.y;
        distanceDelta.y = cellSize / abs(direction.y);
    }

    float transmission = 1.0;
    float travelled = 0.0;
    for (int i = 0; i < 1024; ++i) {
        float segmentEnd =
            min(min(nextDistance.x, nextDistance.y), rayLength);
        float segmentLength = max(segmentEnd - travelled, 0.0);
        if (
            segmentLength > 0.0 &&
            SampleStaticOccupancy(cell) > 0.0
        ) {
            float segmentTransmission = TraceStaticFineSegment(
                start + direction * travelled,
                direction,
                segmentLength
            );
            if (segmentTransmission <= 0.000001) {
                return 0.0;
            }
            transmission *= segmentTransmission;
        }

        travelled = segmentEnd;
        if (travelled >= rayLength - 0.000001) {
            return transmission;
        }
        if (nextDistance.x <= segmentEnd + 0.000001) {
            cell.x += stepDirection.x;
            nextDistance.x += distanceDelta.x;
        }
        if (nextDistance.y <= segmentEnd + 0.000001) {
            cell.y += stepDirection.y;
            nextDistance.y += distanceDelta.y;
        }
    }
    return 0.0;
}

float TraceTransmission(vec2 from, vec2 to) {
    vec2 fullRay = to - from;
    float fullLength = length(fullRay);
    if (fullLength <= 1.0) {
        return 1.0;
    }

    vec2 rayDirection = fullRay / fullLength;
    vec2 start = from + rayDirection * 0.5;
    vec2 finish = to - rayDirection * 0.5;
    vec2 ray = finish - start;
    float rayLength = length(ray);
    if (rayLength <= 0.0) {
        return 1.0;
    }

    vec2 direction = ray / rayLength;
    float transmission = 1.0;
    if (traceStatic > 0.5) {
        transmission =
            TraceStaticHierarchy(start, direction, rayLength);
        if (transmission <= 0.000001) {
            return 0.0;
        }
    }
    if (traceDynamic > 0.5) {
        transmission *=
            TraceDynamicMask(start, direction, rayLength);
    }
    return transmission;
}

vec2 ScreenPixelToWorld() {
    vec2 pixelPosBLView = gl_FragCoord.xy / targetPixelScale;
    vec2 pixelPosTLView =
        vec2(pixelPosBLView.x, screenSize.y - pixelPosBLView.y) -
        mapViewOffset;
    vec2 center = viewPos + screenSize * 0.5;
    return
        center +
        rotate2D(pixelPosTLView - screenSize * 0.5, viewSinCos);
}

void main() {
    vec2 pixelPosTLWorld = ScreenPixelToWorld();
    float distanceToLight =
        length(pixelPosTLWorld - lightPos);
    if (
        lightRadius <= 0.0 ||
        lightIntensity <= 0.0 ||
        distanceToLight >= lightRadius
    ) {
        discard;
    }

    if (useCachedStaticLight > 0.5) {
        vec2 cachedUV =
            (pixelPosTLWorld - (lightPos - vec2(lightRadius))) /
            (lightRadius * 2.0);
        vec3 cachedDirect = texture2D(
            cachedStaticLight,
            vec2(cachedUV.x, 1.0 - cachedUV.y)
        ).rgb;
        float dynamicTransmissionValue = 1.0;
        if (traceDynamic > 0.5) {
            dynamicTransmissionValue = TraceTransmission(
                lightPos,
                pixelPosTLWorld
            );
        }
        gl_FragColor = vec4(
            cachedDirect * dynamicTransmissionValue,
            1.0
        );
        return;
    }

    float radialAttenuation =
        1.0 - distanceToLight / lightRadius;
    float transmission =
        TraceTransmission(lightPos, pixelPosTLWorld);
    vec3 direct =
        lightColor *
        lightIntensity *
        radialAttenuation *
        transmission;
    gl_FragColor = vec4(direct, 1.0);
}
