#include <__msvc_ostream.hpp>
#include <iostream>


#include "RaylibDesktop.h"
#include "raylib.h"

int main()
{
	// Initializes desktop replacement magic
	InitRaylibDesktop();

	// Sets up the desktop (-1 is the entire desktop spanning all monitors)
	MonitorInfo monitorInfo = GetWallpaperTarget(-1);

	// Initialize the raylib window.
	InitWindow(monitorInfo.monitorWidth, monitorInfo.monitorHeight, "Raylib Desktop Demo");

	// Retrieve the handle for the raylib-created window.
	void *raylibWindowHandle = GetWindowHandle();

	// Reparent the raylib window to the window behind the desktop icons.
	RaylibDesktopReparentWindow(raylibWindowHandle);

	// Configure the desktop positioning.
	ConfigureDesktopPositioning(monitorInfo);

	// Now, enter the raylib render loop.
	SetTargetFPS(60);

	// --- Animation variables ---
	float circleX = monitorInfo.monitorWidth / 2.0f;
	float circleY = monitorInfo.monitorHeight / 2.0f;
	float circleRadius = 100.0f;
	float speedX = 4.0f;
	float speedY = 4.5f;

	// Main render loop.
	while (!WindowShouldClose()) {
		// Update the mouse state of the replacement api.
		RaylibDesktopUpdateMouseState();

		// skip rendering if the wallpaper is occluded more than 95%
		if (IsMonitorOccluded(monitorInfo, 0.95)) {
			std::cout << "Wallpaper is occluded" << std::endl;
			WaitTime(0.1);
			continue;
		}

		if (IsDesktopLocked() ) {
			std::cout << "Desktop is locked" << std::endl;
			// If the desktop is locked, we can skip rendering.
			// This is useful to avoid unnecessary rendering when the user is not interacting with the desktop.
			WaitTime(0.1);
			continue;
		}

		// Update the circle's position.
		circleX += speedX;
		circleY += speedY;

		// Bounce off the left/right boundaries.
		if (circleX - circleRadius < 0 || circleX + circleRadius > monitorInfo.monitorWidth)
			speedX = -speedX;
		// Bounce off the top/bottom boundaries.
		if (circleY - circleRadius < 0 || circleY + circleRadius > monitorInfo.monitorHeight)
			speedY = -speedY;

		// Begin the drawing phase.
		BeginDrawing();
		ClearBackground(RAYWHITE);

		// Draw a bouncing red circle.
		DrawCircle((int)circleX, (int)circleY, circleRadius, RED);

		// Attempt to display the mouse position.
		// Note: In a wallpaper window (child of WorkerW), input may not be delivered normally.
		int mouseX = RaylibDesktopGetMouseX();
		int mouseY = RaylibDesktopGetMouseY();

		// check buttons
		if (RaylibDesktopIsMouseButtonDown(0)) {
			DrawCircle(mouseX, mouseY, 10, BLUE);
		}
		else if (RaylibDesktopIsMouseButtonPressed(1)) {
			// exit on right click
			break;
		}

		DrawText(TextFormat("Mouse: %d, %d", mouseX, mouseY), mouseX, mouseY, 30, DARKGRAY);

		DrawFPS(10, 10);
		EndDrawing();
	}

	// Close the window and unload resources.
	CloseWindow();

	// Clean up the desktop window.
	// Restores the original wallpaper.
	CleanupRaylibDesktop();

	return 0;
}
