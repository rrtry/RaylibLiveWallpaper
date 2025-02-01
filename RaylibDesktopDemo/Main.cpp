
#include "RaylibDesktop.h"

#include "raylib.h"

int main()
{
    // Initializes desktop replacement magic
    InitRaylibDesktop();

    // Initialize the raylib window.
    InitWindow(g_desktopWidth, g_desktopHeight, "Raylib Desktop Demo");

    // Retrieve the handle for the raylib-created window.
    void* raylibWindowHandle = GetWindowHandle();

    // Reparent the raylib window to the window behind the desktop icons.
    RaylibDesktopReparentWindow(raylibWindowHandle);

    // Now, enter the raylib render loop.
    SetTargetFPS(60);

    // --- Animation variables ---
    float circleX = g_desktopWidth / 2.0f;
    float circleY = g_desktopHeight / 2.0f;
    float circleRadius = 100.0f;
    float speedX = 4.0f;
    float speedY = 4.5f;

    // Main render loop.
    while (!WindowShouldClose())    // This checks for raylib's internal close event (ESC key, etc.)
    {
        RaylibDesktopUpdateMouseState();

        // Update the circle's position.
        circleX += speedX;
        circleY += speedY;

        // Bounce off the left/right boundaries.
        if (circleX - circleRadius < 0 || circleX + circleRadius > g_desktopWidth)
            speedX = -speedX;
        // Bounce off the top/bottom boundaries.
        if (circleY - circleRadius < 0 || circleY + circleRadius > g_desktopHeight)
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

        //check buttons
        if (RaylibDesktopIsMouseButtonDown(0))
        {
            DrawCircle(mouseX, mouseY, 10, BLUE);
        }
		else if (RaylibDesktopIsMouseButtonPressed(1))
		{
			//exit on right click
            break;
		}

        DrawText(TextFormat("Mouse: %d, %d", mouseX, mouseY), mouseX, mouseY, 30, DARKGRAY);

        EndDrawing();
    }

    // Close the window and unload resources.
    CloseWindow();

	// Clean up the desktop window.
	// Restores the original wallpaper.
    CleanupRaylibDesktop();

    return 0;
}
