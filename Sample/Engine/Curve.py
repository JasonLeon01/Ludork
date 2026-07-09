# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
from typing import Any, Dict, List


class Curve:
    r"""
    \brief Runtime curve asset backed by keyed interpolation data.

    Curve keys use UE-style time/value samples with interpolation mode and optional arrive/leave tangents.
    """

    def __init__(self, data: Dict[str, Any]) -> None:
        r"""
        \brief Construct a curve from asset data.

        - \param data Curve asset dictionary loaded from Data/Curves.
        """
        self.name: str = str(data.get("name", ""))
        self.defaultValue: float = float(data.get("defaultValue", 0.0) or 0.0)
        self.preInfinity: str = self._normaliseInfinityMode(data.get("preInfinity", "constant"))
        self.postInfinity: str = self._normaliseInfinityMode(data.get("postInfinity", "constant"))
        self.keys: List[Dict[str, Any]] = self._normaliseKeys(data.get("keys", []))

    @classmethod
    def fromData(cls, data: Dict[str, Any]) -> "Curve":
        r"""
        \brief Create a curve from asset data.

        - \param data Curve asset dictionary.
        - \return Curve instance.
        """
        return cls(data)

    def toData(self) -> Dict[str, Any]:
        r"""
        \brief Serialise this curve back to asset data.

        - \return Curve asset dictionary.
        """
        return {
            "type": "curve",
            "name": self.name,
            "defaultValue": self.defaultValue,
            "preInfinity": self.preInfinity,
            "postInfinity": self.postInfinity,
            "keys": copy.deepcopy(self.keys),
        }

    def isEmpty(self) -> bool:
        r"""
        \brief Check whether this curve has no keys.

        - \return True when the curve has no keyframes.
        """
        return len(self.keys) == 0

    def getDuration(self) -> float:
        r"""
        \brief Get the time span covered by the first and last key.

        - \return Curve duration in seconds or units.
        """
        if len(self.keys) < 2:
            return 0.0
        return float(self.keys[-1]["time"]) - float(self.keys[0]["time"])

    def evaluate(self, time: float) -> float:
        r"""
        \brief Evaluate the curve at the given time.

        - \param time Time coordinate.
        - \return Interpolated curve value.
        """
        if not self.keys:
            return self.defaultValue
        if len(self.keys) == 1:
            return float(self.keys[0]["value"])
        sampleTime = float(time)
        first = self.keys[0]
        last = self.keys[-1]
        if sampleTime <= float(first["time"]):
            return self._extrapolate(sampleTime, first, self.keys[1], self.preInfinity, True)
        if sampleTime >= float(last["time"]):
            return self._extrapolate(sampleTime, self.keys[-2], last, self.postInfinity, False)
        for index in range(len(self.keys) - 1):
            start = self.keys[index]
            end = self.keys[index + 1]
            startTime = float(start["time"])
            endTime = float(end["time"])
            if startTime <= sampleTime <= endTime:
                return self._evaluateSegment(start, end, sampleTime)
        return float(last["value"])

    def _normaliseInfinityMode(self, value: Any) -> str:
        mode = str(value)
        return mode if mode in ("constant", "linear") else "constant"

    def _normaliseInterpolation(self, value: Any) -> str:
        mode = str(value)
        return mode if mode in ("constant", "linear", "cubic") else "linear"

    def _normaliseKeys(self, value: Any) -> List[Dict[str, Any]]:
        if not isinstance(value, list):
            return []
        keys: List[Dict[str, Any]] = []
        for item in value:
            if not isinstance(item, dict):
                continue
            keys.append(
                {
                    "time": float(item.get("time", 0.0) or 0.0),
                    "value": float(item.get("value", 0.0) or 0.0),
                    "interpolation": self._normaliseInterpolation(item.get("interpolation", "linear")),
                    "arriveTangent": float(item.get("arriveTangent", 0.0) or 0.0),
                    "leaveTangent": float(item.get("leaveTangent", 0.0) or 0.0),
                }
            )
        return sorted(keys, key=lambda key: float(key["time"]))

    def _evaluateSegment(self, start: Dict[str, Any], end: Dict[str, Any], time: float) -> float:
        startTime = float(start["time"])
        endTime = float(end["time"])
        startValue = float(start["value"])
        endValue = float(end["value"])
        duration = endTime - startTime
        if duration <= 0.0:
            return endValue
        interpolation = str(start.get("interpolation", "linear"))
        if interpolation == "constant":
            return startValue
        alpha = (time - startTime) / duration
        if interpolation == "cubic":
            return self._cubicHermite(
                startValue,
                endValue,
                float(start.get("leaveTangent", 0.0) or 0.0) * duration,
                float(end.get("arriveTangent", 0.0) or 0.0) * duration,
                alpha,
            )
        return startValue + (endValue - startValue) * alpha

    def _cubicHermite(
        self,
        startValue: float,
        endValue: float,
        leaveTangent: float,
        arriveTangent: float,
        alpha: float,
    ) -> float:
        t2 = alpha * alpha
        t3 = t2 * alpha
        return (
            (2.0 * t3 - 3.0 * t2 + 1.0) * startValue
            + (t3 - 2.0 * t2 + alpha) * leaveTangent
            + (-2.0 * t3 + 3.0 * t2) * endValue
            + (t3 - t2) * arriveTangent
        )

    def _extrapolate(
        self, time: float, start: Dict[str, Any], end: Dict[str, Any], mode: str, beforeFirst: bool
    ) -> float:
        edgeKey = start if beforeFirst else end
        if mode != "linear":
            return float(edgeKey["value"])
        startTime = float(start["time"])
        endTime = float(end["time"])
        startValue = float(start["value"])
        endValue = float(end["value"])
        duration = endTime - startTime
        if duration <= 0.0:
            return float(edgeKey["value"])
        slope = (endValue - startValue) / duration
        return float(edgeKey["value"]) + (time - float(edgeKey["time"])) * slope
