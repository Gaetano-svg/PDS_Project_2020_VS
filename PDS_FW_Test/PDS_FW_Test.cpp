// FileWatcherTest.cpp : Defines the entry point for the console application.
//

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <conio.h>
#include "CFileSystemWatcher.hpp"

using namespace std;
class FileWatcherListener : public IFileWatcherListener
{
    // Inherited via IFileWatcherListener
    virtual void OnFileChange(const std::wstring& path) override
    {
        cout << "OnFileChange" << endl;
    }

    // Inherited via IFileWatcherListener
    virtual void OnFileAdded(const std::wstring& path) override
    {
        cout << "OnFileAdded" << endl;
    }

    // Inherited via IFileWatcherListener
    virtual void OnFileRemoved(const std::wstring& path) override
    {
        cout << "OnFileRemoved" << endl;
    }

    // Inherited via IFileWatcherListener
    virtual void OnFileRenamed(const std::string& nPath, const std::string& oPath) override
    {
        cout <<  "OnFileRenamed " << nPath << " " << oPath << endl;
    }

};

int main()
{

    /*!
     * Creates watcher by passing the watch directory
     *
     */
    CFileSystemWatcher watcher(L"C:\\Users\\gabuscema\\Desktop\\UserFolder\\OtherFolder\\test");
    /*!
     *
     * Creates Linstener
     */
    IFileWatcherListener* listener = new FileWatcherListener();

    //passes the listener to watcher
    watcher.AddFileChangeListener(listener);
    int choice;
    do
    {
        cout << "\n\nMENU";
        cout << "\n1. Start Directory Watcher";
        cout << "\n2. Stop Directory Watcher";
        cout << "\n3. Exit";
        cout << "\n\nEnter your choice 1-3 :";
        cin >> choice;

        switch (choice)
        {
        case 1:
        {
            if (!watcher.Running())
                watcher.Start();
            else
                cout << "\nWatcher is already started ..";
        }
        break;
        case 2:
            watcher.Stop();
            break;
        case 3:
            exit(0);
            break;
        default:
            cout << "\nInvalid choice";
        }
    } while (choice != 3);


    delete listener;

    return 0;
}