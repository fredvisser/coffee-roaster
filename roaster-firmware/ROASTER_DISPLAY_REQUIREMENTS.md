# Roaster Display Requirements

## Purpose

Define the operator-facing requirements for the onboard roaster display. This document covers the display workflows for roast setup, roast execution, cancellation, cooling, Wi-Fi setup, and profile viewing/selection.

## Scope

The display shall support these operating screens:

- Start
- Network
- Roasting
- Cooling
- Error
- Active Profile

The display requirements apply to any supported display backend. The operator experience should remain consistent whether the implementation is rendered on LVGL hardware or through any future local display backend.

## Design Principles

- The display is the primary local operator interface for starting, monitoring, and canceling a roast.
- The display shall prioritize safety-critical state changes over cosmetic updates.
- The display shall expose profile selection and final-temperature override locally, but full profile authoring remains outside the display UI.
- Local display actions shall not silently corrupt or permanently overwrite saved profiles.

## User Goals

- Select the desired roast profile.
- Review the active profile before starting.
- Set the roast final temperature for the upcoming roast.
- Start a roast from the display without needing the web UI.
- Watch roast progress and key telemetry during roasting.
- Cancel a roast and force a cooling transition.
- Stop cooling and return the machine to idle.
- Configure Wi-Fi credentials and confirm network status.
- Understand error and fault conditions from the display alone.

## Functional Requirements

### FR-1: Start Screen

- The Start screen shall be the default screen when the roaster is idle.
- The Start screen shall display the current bean temperature.
- The Start screen shall display the currently selected profile name.
- The Start screen shall display the configured network address or Wi-Fi status summary.
- The Start screen shall display the firmware revision.
- The Start screen shall display the roast final-temperature target that will be used if the operator starts a roast immediately.
- The Start screen shall provide controls to start a roast, open Wi-Fi configuration, and open the active-profile view.

### FR-2: Final Temperature Setting

- The display shall allow the operator to set or adjust the roast final temperature before starting a roast.
- The display shall show the current value at all times while the roaster is idle.
- The displayed value shall default to the active profile's final target when a profile is loaded or changed.
- The supported range shall be bounded to a safe configured range. The current firmware uses 0-500 F, and the display shall not allow values outside the enforced firmware range.
- Starting a roast shall use the displayed final temperature as the run-time stop target for that roast.
- The final-temperature adjustment shall be treated as a roast-session override unless the operator explicitly saves profile changes through a separate profile-editing flow.

### FR-3: Start Roast

- The display shall allow roast start only when the roaster is in an idle-ready state.
- The display shall block roast start when no valid profile is available.
- The display shall present an explicit error message if the operator attempts to start without a valid profile.
- When a roast is started, the display shall transition to the Roasting screen immediately.
- Starting a roast shall use the active profile for the temperature and fan trajectory.
- Starting a roast shall use the display-selected final-temperature target for roast completion logic.

### FR-4: Roasting Progress

- The Roasting screen shall show the current measured temperature.
- The Roasting screen shall show the current target temperature derived from the active profile.
- The Roasting screen shall show the configured final target temperature.
- The Roasting screen shall show roast progress in time or profile-progress terms.
- The Roasting screen shall show the commanded fan setting.
- The Roasting screen shall show additional run telemetry when available, including heater output, exhaust or fan temperature, and blower drive level.
- Telemetry shall refresh frequently enough for an operator to track roast progression without perceivable lag. A target of at least 1 Hz is required, with faster refresh preferred.

### FR-5: Cancel Roast

- The Roasting screen shall provide a cancel or stop control.
- Canceling a roast shall immediately stop active roast control and de-energize the heater.
- Canceling a roast shall transition the machine into Cooling rather than directly to Idle.
- The system shall record the roast as user-terminated for any downstream logging or reporting path.

### FR-6: Cooling

- The Cooling screen shall show that the roaster is in a cooling state.
- The Cooling screen shall continue to display current temperature.
- The Cooling screen shall show the cooling target or status.
- The Cooling screen shall show that cooling airflow is commanded to maximum or otherwise safe cooling output.
- The Cooling screen shall provide a control to stop cooling and return to Idle when the operator determines it is safe.

### FR-7: Wi-Fi Configuration

- The display shall provide a dedicated Wi-Fi configuration workflow.
- The Network screen shall expose editable fields for SSID and password.
- The Network screen shall provide an apply action.
- While a connection attempt is in progress, the display shall show a transient status such as Connecting.
- After a successful connection, the display shall show the resolved network access address or IP address.
- If no credentials are configured, the display shall show a clear no-network state rather than stale connection data.
- Wi-Fi credentials shall be persisted only after a successful connection attempt.
- Failed connection attempts shall not erase previously working credentials unless the operator explicitly replaces them successfully.

### FR-8: View Active Profile

- The display shall provide an Active Profile screen.
- The Active Profile screen shall show the active profile name.
- The Active Profile screen shall visualize the active roast profile curve.
- The Active Profile screen shall refresh the chart when the active profile changes or when the operator requests a refresh.
- The Active Profile screen shall provide a way to return to the screen for the current machine state.

### FR-9: Switch Profiles

- The display shall allow the operator to switch to the previous or next saved profile from the Active Profile screen.
- Profile switching shall be available only while the roaster is idle.
- Profile switching shall wrap around when the operator moves past the first or last saved profile.
- After a profile change, the display shall update the active profile label, final target temperature, and profile visualization.
- The newly selected profile shall become the active profile used for the next roast.

### FR-10: Error Handling

- The display shall provide a dedicated Error screen.
- The Error screen shall present a short, operator-readable fault message.
- The display shall transition to the Error screen for safety-critical faults such as sensor failure or over-temperature conditions.
- The error presentation shall take precedence over normal navigation until the system enters a safe state.

### FR-11: State-Aware Navigation

- The display shall return to the screen that matches the current machine state when the operator exits auxiliary flows.
- If the roaster is idle, the return destination shall be Start.
- If the roaster is roasting, the return destination shall be Roasting.
- If the roaster is cooling, the return destination shall be Cooling.
- If the roaster is faulted, the return destination shall be Error.

## Out of Scope

- Full roast-profile editing on the display.
- Importing or exporting profiles from the display.
- Advanced PID tuning workflows on the display.
- Historical roast analysis on the display.

## Non-Functional Requirements

- Touch interactions shall be operable with gloved or flour-dusted hands typical of a roasting environment.
- Primary actions such as Start, Stop, and Apply Wi-Fi shall remain visible and understandable without training.
- Safety-related commands shall not depend on network availability.
- The display shall remain usable during background Wi-Fi reconnect attempts.
- Screen transitions and telemetry updates shall not block control-loop timing or watchdog servicing.
- Any backend-specific limitation shall be treated as an implementation gap, not a reason to weaken the operator workflow defined here.

## Acceptance Scenarios

### AS-1: Start a roast with final-temperature override

- Given the roaster is idle and a valid profile is active
- When the operator changes the final temperature and presses Start
- Then the roast starts with the active profile
- And the Roasting screen appears immediately
- And the displayed final target matches the operator-selected override

### AS-2: Cancel an active roast

- Given the roaster is actively roasting
- When the operator presses Stop
- Then the heater output is forced off
- And the machine enters Cooling
- And the Cooling screen is shown

### AS-3: Configure Wi-Fi

- Given the roaster is idle
- When the operator opens the Network screen, enters SSID and password, and presses Apply
- Then the display shows Connecting while the connection is attempted
- And after success the display shows the current network address

### AS-4: Review and switch profiles

- Given the roaster is idle and multiple saved profiles exist
- When the operator opens the Active Profile screen and presses Next or Previous
- Then the active profile changes
- And the profile name, curve, and final target display update to match the new selection

## Implementation Notes

- The current firmware already models the required screens and actions in the display abstraction.
- The current firmware also supports local final-temperature override, profile visualization, idle-only profile cycling, roast stop to cooling, cooling stop to idle, and Wi-Fi credential application.
- A dedicated Network screen is part of the intended requirements even if a specific backend currently routes that action differently.