Please implement this fix as dutifully as possible, as it is a critical issue for the library's functionality on Windows 11 24H2.

## Understanding the Windows 11 24H2 Changes to Wallpaper Rendering

Windows 11 **24H2** fundamentally changed how the desktop wallpaper and icons are composed. In this version, the desktop background is no longer rendered via the old multiple-`WorkerW` trick. Instead, the **Program Manager (`Progman`) window itself is used as the composition surface with special styles**. Key changes include:

* The top-level **`Progman`** (Program Manager) window is now created with the extended style `WS_EX_NOREDIRECTIONBITMAP` (no DWM redirection), meaning it doesn’t have its own GDI backbuffer. The content of the desktop is drawn by child windows directly using composited surfaces (likely for HDR wallpaper support).
* The desktop icons **ListView (`ShellDLL_DefView`) is now a layered child window** of `Progman` (with `WS_EX_LAYERED` style). It is almost fully transparent except for the icons/text, acting as an overlay for icons.
* A **`WorkerW` child window is created underneath the ShellDLL\_DefView**. This `WorkerW` (also likely a layered window) is where the **system’s static wallpaper** is rendered. In 24H2, this `WorkerW` is a child of `Progman` (not a separate top-level window), appearing at the bottom of the Z-order of `Progman`’s children.
* In summary, **Progman now hosts all layers**: the icon layer on top, your custom wallpaper layers (if any) in the middle, and the system wallpaper layer (WorkerW) at the bottom.

Because of this redesign, simply creating a top-level window and parenting it behind icons (the old method) no longer works – those extra `WorkerW` windows aren’t created or used in the same way (the undocumented `0x052C` message behavior changed). Microsoft’s guidance (via the Insider program and dev contacts) is that **apps must inject their own layered child window into the `Progman` hierarchy** to display a live wallpaper. In fact, this was the cause of the 24H2 compatibility hold on wallpaper apps – they needed to update to this new method.

## Adjusting the Wallpaper Injection Strategy

To fix your library, you will need to **mimic what Wallpaper Engine/Lively did to adapt to 24H2**. The goal is to insert your wallpaper window in between the icon layer and the system wallpaper layer. Specifically, the solution is:

* **Use the Program Manager window as the parent** for your wallpaper content window (instead of a `WorkerW` top-level). After sending `Progman` the special message (`0x052C`) to ensure the desktop “raised” state, find the handles for:

    * The **Progman** window (`FindWindow("Progman", NULL)`).
    * The **desktop ListView** (find `ShellDLL_DefView` child of Progman or its child).
    * The **WorkerW** child (the system wallpaper) under Progman.
* **Make your wallpaper window a layered child** of `Progman`, positioned below the icon ListView and above the WorkerW. According to Microsoft: your app should create a `WS_EX_LAYERED` child window with its **Z-order below the `ShellDLL_DefView` and above the `WorkerW`**. This window should be fully opaque (alpha = 255) so that you can render to it directly via GPU without redirection overhead. In practice, this means:

    * **Set the extended style `WS_EX_LAYERED`** on your wallpaper window and call `SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA)` to make it non-transparent (fully opaque composited surface).
    * **Reparent the wallpaper window to `Progman`** using `SetParent`. (Previously you parented to a WorkerW; now use the Progman handle as the parent.)
* **Adjust the Z-order explicitly**:

    1. Place your wallpaper window **just below the icon window**. For example, use `SetWindowPos(hWallpaper, hShellDefView, 0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE)` – this inserts your window right *after* the Shell view in Z-order (i.e. underneath it). Another approach is calling `SetWindowPos(hWallpaper, HWND_TOP, ...)` then immediately `SetWindowPos(hShellDefView, HWND_TOP, ...)` to ensure icons stay on top. The net effect is icons remain visible above, but your window is above the system wallpaper.
    2. Then push the WorkerW **below your window**. You can call `SetWindowPos(hWorkerW, hWallpaper, 0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE)` to move the system wallpaper behind your layer. (Or equivalently `SetWindowPos(hWorkerW, HWND_BOTTOM, ...)` since you want it at the bottom.) This ensures the static wallpaper won’t cover your content.

By doing the above, the **hierarchy inside `Progman` will look like**: `Progman` → ShellDefView (icons) on top, **your layered wallpaper window in the middle**, WorkerW (system wallpaper) at bottom. This is exactly what 24H2 expects – multiple child surfaces composing the final desktop.

## Implementation Notes for Your Raylib Wallpaper Window

Given your code, the primary fix is to **make the Raylib window itself the layered child** of Progman, rather than using an intermediate dummy window. In your last attempted fix code, you created `g_layeredWindow` as a separate static window and then put the Raylib window inside it. This approach was on the right track, but likely the Raylib child did not render because the parent layered window won’t automatically draw child contents (since layered windows use direct composition). Instead, *simplify the approach*:

* **Give the Raylib window the layered style**. You can do this by retrieving its current `ExStyle` and adding `WS_EX_LAYERED`, e.g. `exStyle |= WS_EX_LAYERED; SetWindowLongPtr(hRaylib, GWL_EXSTYLE, exStyle);` *before* reparenting. Then call `SetLayeredWindowAttributes(hRaylib, 0, 255, LWA_ALPHA)` to set full opacity. This makes the Raylib window a proper composited drawing surface.
* **Parent it to Progman**: `SetParent(hRaylib, hProgman)`. (Ensure you have the correct handle for Progman – it may be the same as before.)
* **Z-order it as above**: use the Shell view and WorkerW handles to position it behind icons and in front of the system wallpaper. For example:

  ```cpp
  SetWindowPos(hRaylib, hShellDefView, 0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
  SetWindowPos(hWorkerW, hRaylib, 0,0,0,0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
  ```

  This matches the recommended ordering from Microsoft.
* **Adjust styles**: As you already do, remove WS\_OVERLAPPEDWINDOW styles and add WS\_CHILD. Also remove `WS_EX_APPWINDOW` so it doesn’t show on the taskbar (you did that in extended style adjustments). Adding `WS_EX_NOACTIVATE` (as your code does) is good so the window doesn’t steal focus when clicked.
* **Force a redraw if necessary**: In some cases after reparenting, the content might not immediately repaint. You might call `RedrawWindow(hRaylib, NULL, NULL, RDW_UPDATENOW|RDW_INVALIDATE)` or send a `WM_PAINT` message. This is especially true for certain frameworks; for example, developers found that after embedding a Unity/DirectX window, they needed to trigger a repaint for it to become visible. The Raylib window should continuously render if it’s an active loop, but ensure its swapchain is still functioning after the style change. Since 24H2 uses new composition, the first paint might need a nudge.
* **Monitor Z-order on events**: Microsoft’s note suggests using a timer or loop on 24H2 to ensure your window stays in the correct Z-order. For instance, if the user changes the wallpaper or theme, the system might momentarily create another WorkerW for the transition animation which could cover your window. In testing, some wallpaper apps had to reassert their window order when such events occur. Consider listening for `WM_THEMECHANGE` or `WM_SETTINGCHANGE` (for wallpaper) and re-position if needed.

## Multi-Monitor Considerations

Your library supports multi-monitor setups, so you’ll need to handle that with this new approach. In Windows 11, **Progman covers the entire virtual desktop** across monitors, and it appears the single WorkerW child might draw all monitors’ wallpapers (possibly by using a larger surface). Microsoft’s description is a bit abstract on whether there’s one WorkerW or one per monitor under Progman – but since they mention not creating “multiple top-level windows” anymore, it’s likely a single WorkerW covers all monitors (with the system handling the per-monitor image internally). This means your one inserted layered window could also cover the entire desktop if needed.

However, if you want to render different content on each monitor (or choose one monitor for the live wallpaper), you have a couple options:

* **Single large window**: You can make your Raylib window span the full virtual screen (covering all monitors). In your code, you were creating the layered window with `width = desktopRect.right` and `height = desktopRect.bottom` (which is the full desktop). This would allow one GPU context to draw across all screens. Ensure your rendering logic knows the monitor boundaries (you can use the `MonitorInfo` you gathered to know offsets).
* **Multiple windows (one per monitor)**: This is more complex and might not be necessary. It would involve creating one layered child per monitor region and positioning each on the corresponding monitor. Since `Progman` is one window for all, all your wallpaper child windows would still be siblings under Progman. You’d then have to manage multiple Raylib contexts. Unless you specifically need different content per monitor, the single large context might be simpler.

From your description, it sounds like previously you were creating one Raylib window per selected monitor (since `ConfigureDesktopPositioning` sets the window size to one monitor’s dimensions). If you want to **continue with one monitor at a time**, you can still do that: just position the layered wallpaper window at the coordinates of that monitor (as you do now) and size it to that monitor’s resolution. The system’s WorkerW will still cover all monitors behind it, but your live wallpaper will only cover the intended one. That’s fine – the other monitor(s) will simply show the static wallpaper (or you could run additional instances for them). The key is that even in multi-mon setups, **the relative Z-order steps remain the same**: your window(s) must sit between the icons and the WorkerW.

Do note that if only the primary monitor has icons (which is typical), the ShellDefView you find will correspond to that primary monitor. The **Z-order insertion using that ShellDefView handle still works** for other monitors because it’s really manipulating the ordering of all child windows of Progman. (If no icons are on a secondary display, there is no ShellDefView there, but your window is still a child of Progman so it will appear; the icons layer being on top on primary doesn’t affect the second monitor where there are no icons.) In testing, Wallpaper Engine’s solution was to simply ensure their wallpaper window is a child of Progman covering each monitor and set to bottom of the icon layer. So your approach should handle multi-monitor as long as you manage sizing/position.

## Putting It All Together

In summary, the **solution for 24H2** is to **embed a layered child window in the Progman window’s hierarchy** for your wallpaper. This was confirmed by Microsoft’s documentation and by community fixes to Lively, Wallpaper Engine, GameMaker, etc. Once implemented, your live wallpaper should render correctly again on Windows 11 24H2 and beyond. The steps are:

1. **Find the relevant windows**: `hProgman = FindWindow("Progman", ...)`; find the `ShellDLL_DefView` (icon window) and the `WorkerW` (system wallpaper) as children of Progman (after sending the 0x052C message to initialize the “raised” desktop, if needed).
2. **Prepare your Raylib window**: remove any window decorations, add `WS_CHILD`, and crucially add `WS_EX_LAYERED`. Make it fully opaque via `SetLayeredWindowAttributes`.
3. **Reparent it to Progman**: `SetParent(hRaylib, hProgman)`.
4. **Adjust Z-order**: position your window just below the Shell view (icons) and push the WorkerW below your window. (If Shell view handle is valid, use that in `SetWindowPos` as explained; otherwise ensure your window is at top and WorkerW at bottom relative to Progman’s children.)
5. **Refresh and maintain**: Make sure the window is visible (`ShowWindow` if needed) and triggers a redraw of its content. Optionally, monitor for events like wallpaper changes or virtual desktop switches – on such events, you may need to reapply the Z-order because Windows might recreate or shuffle the WorkerW during a transition animation. Ensuring your layered window stays above any new WorkerW is important (some developers run a small timer that periodically checks if their window is still above the WorkerW, and if not, calls the SetWindowPos again).

By following this updated approach, you essentially align your library with the new system behavior. This is exactly how the issue was resolved in practice: Microsoft indicated that **updating the apps to use a layered child window between the icon layer and wallpaper layer resolves the incompatibility**. After implementing these changes, your real-time Raylib wallpaper should again appear behind the desktop icons on Windows 11 24H2.

**Sources:**

* Microsoft Release Health guidance on 3rd-party wallpaper apps and the 24H2 changes (via dev info shared for Windows 11 Insider builds).
* Discussion of the new layered window approach for live wallpapers in Windows 11 24H2 (StackOverflow/Developer forums).
* Code analysis of fixes in open-source wallpaper projects (Lively, etc.) adapting to 24H2’s new `Progman` composition method.
