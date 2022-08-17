using Microsoft.Win32;
using MultikeysEditor.Domain;
using MultikeysEditor.Domain.BackgroundRunner;
using MultikeysEditor.Model;
using MultikeysEditor.View.Controls;
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Imaging;


namespace MultikeysEditor
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private System.Windows.Forms.NotifyIcon ni;

        private string _lastLayoutPath = AppDomain.CurrentDomain.BaseDirectory + @"\" + "lastLayout.txt";
        public MainWindow()
        {
            // Subscribe to any unhandled exception
            System.AppDomain.CurrentDomain.UnhandledException += HandleAnyException;

            InitializeComponent();

            // Update menu buttons right away
            EnableDisableMenuButtons();

            // Setup the core runner
            multikeysCoreRunner = new MultikeysCoreRunner();

            //Create previous layout file
            if(!System.IO.File.Exists(_lastLayoutPath)) File.WriteAllText(_lastLayoutPath, "");
            //Attempt to load last file
            string lastLayout = System.IO.File.ReadAllText(_lastLayoutPath);
            //Attempt to open file if there is a previous layout file path
            if (!string.IsNullOrWhiteSpace(lastLayout))
            {
                try
                {
                    var layout = new DomainFacade().LoadLayout(lastLayout);
                    workingFileName = lastLayout;
                    // and replace the layout control
                    layoutControl = new LayoutControl();
                    layoutControl.LoadLayout(layout);
                    DockPanelLayout.Children.Clear();
                    DockPanelLayout.Children.Add(layoutControl);
                    new DomainFacade().SaveLayout(layoutControl.GetLayout(), workingFileName);
                    EnableDisableMenuButtons();
                }
                catch
                {
                    //The file wasn't found or was invalid
                }
            }
            
            
            //Launch on startup logic
            Microsoft.Win32.RegistryKey key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
            string str = Assembly.GetExecutingAssembly().Location;
            key.SetValue("Multikeys", str);
            
            //Minimize logic
            ni = new System.Windows.Forms.NotifyIcon();
            //NOTE: An icon file must be present in the release folder
            ni.Icon = new System.Drawing.Icon("Multikeys_Logo.ico");
            ni.DoubleClick +=
                delegate (object sender, EventArgs args)
                {
                    ni.Visible = false;
                    this.Show();
                    this.WindowState = WindowState.Normal;
                };

        }
        protected override void OnStateChanged(EventArgs e)
        {
            if (WindowState == System.Windows.WindowState.Minimized)
            {
                ni.Visible = true;
                this.Hide();
            }
            base.OnStateChanged(e);
        }
        private void HandleAnyException(object sender, UnhandledExceptionEventArgs e)
        {
            string logDirectory = System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetEntryAssembly().Location) + "\\..\\log";

            // Create the log folder if it does not exist
            if (!Directory.Exists(logDirectory))
                try
                {
                    Directory.CreateDirectory(logDirectory);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(
                        Properties.Strings.LogFailedToCreateLog + $" {ex.Message}{Environment.NewLine}{(e.ExceptionObject as Exception).Message}",
                        Properties.Strings.Error,
                        MessageBoxButton.OK,
                        MessageBoxImage.Error
                        );
                    Close();
                }

            // Create a log file at the log folder
            try
            {
                var now = DateTime.Now;
                // Make the file name using today's date
                string logPath = logDirectory + $"\\{now.ToString("yyyy_MM_dd")}.log";

                // Append the log to the file; if the file does not exist, it is created.
                string logText =
                    $"Exception occured at {now.Hour:D2}:{now.Minute:D2}:{now.Second:D2}, " +
                    $"{now.Day:D2} of {now.ToString("MMMM", CultureInfo.InvariantCulture)}, {now.Year:D4}" +
                    Environment.NewLine +
                    (e.ExceptionObject as Exception).Message + Environment.NewLine +
                    (e.ExceptionObject as Exception).StackTrace +
                    new string(Enumerable.Range(0, 5).SelectMany(x => Environment.NewLine).ToArray());    // <- five line breaks

                File.AppendAllText(logPath, logText);

                // Warn the user
                MessageBox.Show(
                    Properties.Strings.LogSeeLog,
                    Properties.Strings.Warning,
                    MessageBoxButton.OK,
                    MessageBoxImage.Warning);
                Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    Properties.Strings.LogFailedToWriteToLog + $" {ex.Message}{Environment.NewLine}{(e.ExceptionObject as Exception).Message}",
                    Properties.Strings.Error,
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                Close();
            }
        }

        /// <summary>
        /// The currently opened file; null if no file is open.
        /// If this variable contains an empty string, then the file being
        /// edited has not been saved yet.
        /// </summary>
        private string workingFileName = null;

        /// <summary>
        /// The control that represents the layout being currently edited.
        /// </summary>
        private LayoutControl layoutControl = null;



        #region Menu

        private void FileNew_Click(object sender, EventArgs e)
        {
            // If there is already a file being edited
            if (workingFileName != null)
            {
                // Ask the user if they want to discard the open file
                var result = MessageBox.Show(Properties.Strings.WarningWillOverwriteWorkingLayout,
                    Properties.Strings.Warning,
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);
                if (result != MessageBoxResult.Yes)
                { return; }
            }

            // Make a brand new layout instance, with a keyboard in it
            var newLayout = new MultikeysLayout();
            newLayout.Keyboards.Add(new Model.Keyboard());
            // empty string means layout that hasn't been saved yet
            workingFileName = "";
            // Place a new layout control in the dock panel
            layoutControl = new LayoutControl();
            layoutControl.LoadLayout(newLayout);
            DockPanelLayout.Children.Clear();
            DockPanelLayout.Children.Add(layoutControl);

            EnableDisableMenuButtons();
        }

        private void FileSave_Click(object sender, EventArgs e)
        {
            // Can't save a layout if there's none being edited
            if (workingFileName == null)
            {
                MessageBox.Show(Properties.Strings.WarningNoLayout, Properties.Strings.Warning, MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            else if (workingFileName == "")
            {
                // Empty string means the file hasn't been saved yet
                // Do the same as "save as"
                FileSaveAs_Click(sender, e);
                return;
            }
            else
            {
                new DomainFacade().SaveLayout(layoutControl.GetLayout(), workingFileName);
            }

            EnableDisableMenuButtons();
        }

        private void FileSaveAs_Click(object sender, EventArgs e)
        {
            // Can't save a layout if there's none being edited
            if (workingFileName == null)
            {
                MessageBox.Show(Properties.Strings.WarningNoLayout, Properties.Strings.Warning, MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            // Prompt for a filename to save
            var saveFileDialog = new SaveFileDialog();
            if (saveFileDialog.ShowDialog() == true)
            {
                // retrieve layout and save it
                new DomainFacade().SaveLayout(layoutControl.GetLayout(), saveFileDialog.FileName);
                // then, the new filenamed is used as the current file being edited
                workingFileName = saveFileDialog.FileName;
            }
            SaveLastLayout();
            EnableDisableMenuButtons();
        }

        private void FileOpen_Click(object sender, EventArgs e)
        {
            // If there is a layout being edited
            if (workingFileName != null)
            {
                // Warn the user that their changes will be lost, prompt for cancellation
                var result = MessageBox.Show(Properties.Strings.WarningWillOverwriteWorkingLayout,
                    Properties.Strings.Warning,
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);
                if (result != MessageBoxResult.Yes)
                { return; }
            }

            // Ask for a file to open
            var openFileDialog = new OpenFileDialog();
            if (openFileDialog.ShowDialog() == true)
            {
                // TODO: Add exception handling when opening files
                var layout = new DomainFacade().LoadLayout(openFileDialog.FileName);
                // remember what file is being worked on
                workingFileName = openFileDialog.FileName;
                // and replace the layout control
                layoutControl = new LayoutControl();
                layoutControl.LoadLayout(layout);
                DockPanelLayout.Children.Clear();
                DockPanelLayout.Children.Add(layoutControl);
            }
            SaveLastLayout();
            EnableDisableMenuButtons();
        }

        private void FileImportKeyboard_Click(object sender, EventArgs e)
        {
            // If there's no file to import into:
            if (workingFileName == null)
            {
                MessageBox.Show(Properties.Strings.WarningNoLayout, Properties.Strings.Warning, MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            // Ask for the file where keyboard will be imported from
            var openFileDialog = new OpenFileDialog();
            if (openFileDialog.ShowDialog() == true)
            {
                var layout = new DomainFacade().LoadLayout(openFileDialog.FileName);
                // give this layout to the current layout to add the keyboards there
                layoutControl.AddKeyboards(layout);
            }
        }

        private void FileClose_Click(object sender, EventArgs e)
        {
            // If there's no file to close:
            if (workingFileName == null)
            {
                MessageBox.Show(Properties.Strings.WarningNoLayout, Properties.Strings.Warning, MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            // Confirm closing
            var result = MessageBox.Show(Properties.Strings.WarningWillOverwriteWorkingLayout,
                    Properties.Strings.Warning,
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);
            if (result == MessageBoxResult.No)
            { return; }

            // Just remove the current layout
            layoutControl = null;
            DockPanelLayout.Children.Clear();

            EnableDisableMenuButtons();
        }


        /// <summary>
        /// Although the menu items event handlers have validation in them, it's useful to disable actions that the user
        /// should not do, such as closing the current layout when there is no open layout.
        /// </summary>
        private void EnableDisableMenuButtons()
        {
            // when there's no open layout
            if (workingFileName == null)
            {
                MenuFileNew.IsEnabled = true;
                MenuFileSave.IsEnabled = false;
                MenuFileSaveAs.IsEnabled = false;
                MenuFileOpen.IsEnabled = true;
                MenuFileClose.IsEnabled = false;
                MenuImportKeyboard.IsEnabled = false;
            }
            // when there's an open layout, all options are enabled
            else
            {
                MenuFileNew.IsEnabled = true;
                MenuFileSave.IsEnabled = true;
                MenuFileSaveAs.IsEnabled = true;
                MenuFileOpen.IsEnabled = true;
                MenuFileClose.IsEnabled = true;
                MenuImportKeyboard.IsEnabled = true;
            }
        }

        private void SaveLastLayout()
        {
            File.WriteAllText(_lastLayoutPath, workingFileName);
        }
        #endregion



        #region BackgroundRunner

        /// <summary>
        /// The object that actually controls the background process. If this application quits,
        /// the background process is automatically killed.
        /// </summary>
        private MultikeysCoreRunner multikeysCoreRunner;

        private void BackgroundRunnerIconStart_Loaded(object sender, RoutedEventArgs e)
        {
            var uriSource = new Uri(@"Resources/StateRunning.png", UriKind.Relative);

            (sender as Image).Source = new BitmapImage(uriSource);
        }

        private void BackgroundRunnerIconStop_Loaded(object sender, RoutedEventArgs e)
        {
            var uriSource = new Uri(@"Resources/StateStopped.png", UriKind.Relative);

            (sender as Image).Source = new BitmapImage(uriSource);
        }


        private void BackgroundRunnerIconStart_MouseUp(object sender, MouseButtonEventArgs e)
        {
            // If no layout is being edited, show a warning and return.
            if (workingFileName == null)
            {
                MessageBox.Show(Properties.Strings.WarningNoLayout,
                    Properties.Strings.Warning,
                    MessageBoxButton.OK,
                    MessageBoxImage.Warning);
                return;
            }

            // If the file name does not yet exist in disk (that is, not yet saved), then warn the user
            // and save it.
            if (!File.Exists(workingFileName))
            {
                var result = MessageBox.Show(
                    Properties.Strings.WarningSaveLayoutToStart,
                    Properties.Strings.Warning,
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);
                if (result == MessageBoxResult.Yes)
                {
                    FileSaveAs_Click(null, null);
                }
                else return;
            }
            multikeysCoreRunner.Start(workingFileName);
            BackgroundRunnerIconStart.Visibility = Visibility.Hidden;
            BackgroundRunnerIconStop.Visibility = Visibility.Visible;

            LabelCurrentCoreState.Content = Properties.Strings.CoreStateRunning;
        }

        private void BackgroundRunnerIconStop_MouseUp(object sender, MouseButtonEventArgs e)
        {
            multikeysCoreRunner.Stop();
            BackgroundRunnerIconStart.Visibility = Visibility.Visible;
            BackgroundRunnerIconStop.Visibility = Visibility.Hidden;

            LabelCurrentCoreState.Content = Properties.Strings.CoreStateStopped;
        }


        #endregion


    }
}
