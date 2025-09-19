# Golf Scorecard
Keep golf scores for up to four players on the Flipper Zero.

## Features
- Quick score adjustment with the D-pad (Up/Down to change strokes, Left/Right to change holes)
- Track 9 or 18 hole rounds and see totals plus relation to par
- Rename players and reset rounds from the Round Setup menu
- Clean GolfScore code structure (`app.cpp`, `scorecard/scorecard.cpp`, `settings/settings.cpp`)

## Usage
1. Open the app and choose **Round Setup** to pick player count, hole count, or rename players.
2. Select **Scorecard** to track the current round. Press **OK** to switch between players.
3. Long-press **OK** on a hole to clear the recorded strokes for that player and hole.

## Notes
- Scores and player names are stored under `/ext/apps_data/golf_score/data/state.bin`.
- Customise the launcher icon by editing `app.png` (10×10, monochrome).
