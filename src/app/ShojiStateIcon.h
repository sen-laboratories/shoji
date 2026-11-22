/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 SEN Labs e.U.
 */
#pragma once

#include <string>

class BBitmap;

/**
 * The icon of a state (`info`, `warning`, `error`, `ok`: the values of `state_field` of the schema, SenSettingsForm.h) as high as @p size: the alert icons of the system (the ones that
 * BAlert shows), and a green check mark for `ok`. The caller owns the bitmap; NULL if the state has no icon (or none could be made).
 */
BBitmap* ShojiStateIcon(const std::string& state, float size);
