# ArchHelp

**Warning**: this installer performs destructive actions on disks. Make sure
you understand what it does before running it.

## Running the installer

The application expects to have root privileges because most of the operations
(partitioning, formatting and mounting) rely on `pkexec`/`sudo`. Launch the
binary with `sudo` or `pkexec`:

```bash
sudo ./ArchHelp
```

Without elevated privileges the formatting step will fail and the ISO will not
be copied to `/mnt`, which results in the "Arch Linux ISO not found" error on
the final page.

1. **Download ISO** – press the *Download* button and wait for the progress
   bar to complete.
2. **Prepare Drive** – select the target disk and click *Prepare Drive*.
3. **Create Default Partitions** – optional helper for creating a simple
   layout.
4. **Continue through the wizard** – follow the prompts to mount the ISO and
   install the base system.

Make absolutely sure you selected the correct drive – the installer will wipe
it completely.
