using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Linq;

namespace Ludork;

public partial class App
{
    private NativeMenu? trayMenu;
    private NativeMenu? dockMenu;
    private IDisposable? trayPopupOpened;

    private void initializeDesktopMenus()
    {
        if (editorSettings is null || (!OperatingSystem.IsMacOS() && !OperatingSystem.IsWindows()))
            return;
        editorSettings.removeMissingRecentProjects();
        trayMenu = new NativeMenu();
        trayMenu.NeedsUpdate += onDesktopMenuNeedsUpdate;
        if (OperatingSystem.IsMacOS())
        {
            dockMenu = new NativeMenu();
            dockMenu.NeedsUpdate += onDesktopMenuNeedsUpdate;
            NativeDock.SetMenu(this, dockMenu);
        }
        refreshDesktopMenus(this, EventArgs.Empty);
        TrayIcon icon = new()
        {
            Icon = EditorWindowIcon.Shared,
            ToolTipText = "Ludork",
            Menu = trayMenu,
        };
        if (OperatingSystem.IsWindows())
        {
            icon.Clicked += (_, _) => restoreWindow();
            trayPopupOpened = Window.WindowOpenedEvent.AddClassHandler<Window>((window, _) =>
            {
                if (window.Content is ItemsControl presenter && ReferenceEquals(presenter.ItemsSource, trayMenu.Items))
                    editorSettings.removeMissingRecentProjects();
            });
        }
        TrayIcon.SetIcons(this, [icon]);
        editorSettings.RecentProjectsChanged += refreshDesktopMenus;
        LocaleService.LanguageChanged += refreshDesktopMenus;
    }

    private void disposeDesktopMenus()
    {
        if (trayMenu is null)
            return;
        if (editorSettings is not null)
            editorSettings.RecentProjectsChanged -= refreshDesktopMenus;
        LocaleService.LanguageChanged -= refreshDesktopMenus;
        trayPopupOpened?.Dispose();
        trayPopupOpened = null;
        trayMenu.NeedsUpdate -= onDesktopMenuNeedsUpdate;
        if (dockMenu is not null)
        {
            dockMenu.NeedsUpdate -= onDesktopMenuNeedsUpdate;
            NativeDock.SetMenu(this, null);
            dockMenu = null;
        }
        TrayIcon.SetIcons(this, null);
        trayMenu = null;
    }

    private void onDesktopMenuNeedsUpdate(object? sender, EventArgs args) =>
        editorSettings?.removeMissingRecentProjects();

    private void refreshDesktopMenus(object? sender, EventArgs args)
    {
        if (editorSettings is null || trayMenu is null)
            return;
        populateDesktopMenu(trayMenu, editorSettings, true);
        if (dockMenu is not null)
            populateDesktopMenu(dockMenu, editorSettings, false);
    }

    private void populateDesktopMenu(NativeMenu menu, EditorSettings settings, bool includeQuit)
    {
        menu.Items.Clear();
        NativeMenu recents = new();
        RecentProjectViewModel[] projects = settings.RecentProjectPaths.Select(path => new RecentProjectViewModel(path)).ToArray();
        foreach (RecentProjectViewModel project in projects)
        {
            bool duplicateName = projects.Count(other => string.Equals(project.Name, other.Name, StringComparison.OrdinalIgnoreCase)) > 1;
            NativeMenuItem item = new(duplicateName ? project.ProjectFilePath : project.Name)
            {
                ToolTip = project.ProjectFilePath,
            };
            item.Click += (_, _) =>
            {
                if (File.Exists(project.ProjectFilePath))
                    openExternalProject(project.ProjectFilePath);
                else
                    settings.removeMissingRecentProjects();
            };
            recents.Add(item);
        }
        menu.Add(new NativeMenuItem(LocaleService.Get("DESKTOP_RECENT"))
        {
            Menu = recents,
            IsEnabled = projects.Length > 0,
        });
        addMenuItem(menu, "NEW_WINDOW", (_, _) => newWindow());
        addMenuItem(menu, "OPEN_PROJECT_DIALOG", async (_, _) => await pickExternalProjectAsync());
        menu.Add(new NativeMenuItemSeparator());
        NativeMenuItem keepRunning = addMenuItem(menu, "KEEP_RUNNING_IN_BACKGROUND", (_, _) =>
        {
            settings.KeepRunningInBackground = !settings.KeepRunningInBackground;
            settings.Save();
            refreshDesktopMenus(this, EventArgs.Empty);
            retentionChanged();
        });
        keepRunning.ToggleType = MenuItemToggleType.CheckBox;
        keepRunning.IsChecked = settings.KeepRunningInBackground;
        if (includeQuit)
            addMenuItem(menu, "QUIT_LUDORK", (_, _) => requestQuit());
    }

    private static NativeMenuItem addMenuItem(NativeMenu menu, string localeKey, EventHandler handler)
    {
        NativeMenuItem item = new(LocaleService.Get(localeKey));
        item.Click += handler;
        menu.Add(item);
        return item;
    }
}
