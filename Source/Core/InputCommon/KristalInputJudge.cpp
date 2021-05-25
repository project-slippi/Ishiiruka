#include "KristalInputJudge.h"
#include "GCPadStatus.h"

#define coord(x) ((u8)(128 + 80*x + 0.5))

bool isKristalInput(const GCPadStatus& newPad, const GCPadStatus& oldPad)
{
	return
	    // Digital (!A -> A, !B -> B, !other <-> other)
	    ((newPad.button & PAD_BUTTON_A) && !(oldPad.button & PAD_BUTTON_A)) ||
	    ((newPad.button & PAD_BUTTON_B) && !(oldPad.button & PAD_BUTTON_B)) ||
	    ((newPad.button & PAD_BUTTON_X) && !(oldPad.button & PAD_BUTTON_X)) ||
	    ((oldPad.button & PAD_BUTTON_X) && !(newPad.button & PAD_BUTTON_X)) ||
	    ((newPad.button & PAD_BUTTON_Y) && !(oldPad.button & PAD_BUTTON_Y)) ||
	    ((oldPad.button & PAD_BUTTON_Y) && !(newPad.button & PAD_BUTTON_Y)) ||
	    ((newPad.button & PAD_TRIGGER_L) && !(oldPad.button & PAD_TRIGGER_L)) ||
	    ((oldPad.button & PAD_TRIGGER_L) && !(newPad.button & PAD_TRIGGER_L)) ||
	    ((newPad.button & PAD_TRIGGER_R) && !(oldPad.button & PAD_TRIGGER_R)) ||
	    ((oldPad.button & PAD_TRIGGER_R) && !(newPad.button & PAD_TRIGGER_R)) ||
	    ((newPad.button & PAD_TRIGGER_Z) && !(oldPad.button & PAD_TRIGGER_Z)) ||
	    ((oldPad.button & PAD_TRIGGER_Z) && !(newPad.button & PAD_TRIGGER_Z)) ||
	    ((newPad.button & PAD_BUTTON_START) && !(oldPad.button & PAD_BUTTON_START)) ||

		// Stick (|X|>=.2875, |X|>=0.8, |Y|>=.2875, |Y|>=0.6625)
	    ((oldPad.stickX < coord(0.2875)) && (newPad.stickX >= coord(0.2875))) ||   // Right
	    ((oldPad.stickX < coord(0.8)) && (newPad.stickX >= coord(0.8))) ||         // Dash right
	    ((oldPad.stickX > coord(-0.2875)) && (newPad.stickX <= coord(-0.2875))) || // Left
	    ((oldPad.stickX > coord(-0.8)) && (newPad.stickX <= coord(-0.8))) ||       // Dash left
	    ((oldPad.stickY < coord(0.2875)) && (newPad.stickY >= coord(0.2875))) ||   // Up
	    ((oldPad.stickY < coord(0.6625)) && (newPad.stickY >= coord(0.6625))) ||   // Jump
	    ((oldPad.stickY > coord(-0.2875)) && (newPad.stickY <= coord(-0.2875))) || // Down
	    ((oldPad.stickY > coord(-0.7)) && (newPad.stickY <= coord(-0.7))) ||       // Crouch
		// Tap jump during dash = ? //TODO?

	    // C Stick
	    ((oldPad.substickX < coord(0.2875))  && (newPad.substickX >= coord(0.2875))) ||   // Right air
	    ((oldPad.substickX < coord(0.8))     && (newPad.substickX >= coord(0.8))) ||         // Right smash
	    ((oldPad.substickX > coord(-0.2875)) && (newPad.substickX <= coord(-0.2875))) || // Left air
	    ((oldPad.substickX > coord(-0.8))    && (newPad.substickX <= coord(-0.8))) ||       // Left smash
	    ((oldPad.substickY < coord(0.2875))  && (newPad.substickY >= coord(0.2875))) ||   // Up air
	    ((oldPad.substickY < coord(0.6625))  && (newPad.substickY >= coord(0.6625))) ||   // Up smash
	    ((oldPad.substickY > coord(-0.2875)) && (newPad.substickY <= coord(-0.2875))) || // Down air
	    ((oldPad.substickY > coord(-0.6625)) && (newPad.substickY <= coord(-0.6625))) || // Down smash

	    // Triggers (no shield -> minimum shield check for both triggers)
	    ((oldPad.triggerLeft < 43) && (newPad.triggerLeft >= 43)) ||
	    ((oldPad.triggerRight < 43) && (newPad.triggerRight >= 43))

		// Dpad (none)
	    ;
}