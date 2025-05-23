import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import queue

class PokemonTraderApp:
    def __init__(self, root_window):
        self.root = root_window
        self.root.title("RP2040 Pokémon Trader UI")
        self.root.geometry("800x700")

        self.serial_port = None
        self.serial_queue = queue.Queue()
        self.is_connected = False
        self.selected_pokemon_index_for_trade = None # Internal tracking of listbox selection for trade command
        self.selected_pokemon_listbox_idx = None # For highlighting in listbox

        # --- Serial Connection Frame ---
        connection_frame = ttk.LabelFrame(self.root, text="Serial Connection", padding="10")
        connection_frame.pack(side=tk.TOP, fill=tk.X, padx=10, pady=5)

        self.port_label = ttk.Label(connection_frame, text="Port:")
        self.port_label.pack(side=tk.LEFT, padx=5)

        self.port_combobox = ttk.Combobox(connection_frame, state="readonly", width=30)
        self.port_combobox.pack(side=tk.LEFT, padx=5)
        self.populate_serial_ports()

        self.connect_button = ttk.Button(connection_frame, text="Connect", command=self.toggle_serial_connection)
        self.connect_button.pack(side=tk.LEFT, padx=5)

        self.connection_status_label = ttk.Label(connection_frame, text="Status: Not Connected", foreground="red")
        self.connection_status_label.pack(side=tk.LEFT, padx=5)

        # --- Main Content Area (PanedWindow for resizable sections) ---
        main_paned_window = tk.PanedWindow(self.root, orient=tk.HORIZONTAL, sashrelief=tk.RAISED)
        main_paned_window.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # Left Pane (Pokemon List & Controls)
        left_pane = ttk.Frame(main_paned_window, padding="5")
        main_paned_window.add(left_pane, weight=1)

        # Pokemon List Frame
        list_frame = ttk.LabelFrame(left_pane, text="Stored Pokémon", padding="10")
        list_frame.pack(fill=tk.BOTH, expand=True, pady=5)

        self.pokemon_listbox = tk.Listbox(list_frame, height=15, exportselection=False)
        self.pokemon_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.pokemon_listbox.bind('<<ListboxSelect>>', self.on_pokemon_list_select)
        
        list_scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.pokemon_listbox.yview)
        list_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.pokemon_listbox.config(yscrollcommand=list_scrollbar.set)

        # Controls Frame
        controls_frame = ttk.LabelFrame(left_pane, text="Controls", padding="10")
        controls_frame.pack(fill=tk.X, pady=10)

        self.refresh_button = ttk.Button(controls_frame, text="Refresh List", command=self.refresh_pokemon_list, state=tk.DISABLED)
        self.refresh_button.grid(row=0, column=0, padx=5, pady=5, sticky="ew")

        self.select_trade_button = ttk.Button(controls_frame, text="Select This Pokémon for Trade", command=self.select_pokemon_for_trade_action, state=tk.DISABLED)
        self.select_trade_button.grid(row=0, column=1, padx=5, pady=5, sticky="ew")
        
        self.initiate_trade_button = ttk.Button(controls_frame, text="Initiate Trade", command=self.initiate_trade_action, state=tk.DISABLED)
        self.initiate_trade_button.grid(row=1, column=0, padx=5, pady=5, sticky="ew")

        self.get_status_button = ttk.Button(controls_frame, text="Get Status", command=self.get_status_action, state=tk.DISABLED)
        self.get_status_button.grid(row=1, column=1, padx=5, pady=5, sticky="ew")

        # Right Pane (Status & Log)
        right_pane = ttk.Frame(main_paned_window, padding="5")
        main_paned_window.add(right_pane, weight=2)

        # Status Display Frame
        status_frame = ttk.LabelFrame(right_pane, text="Device Status & Responses", padding="10")
        status_frame.pack(fill=tk.X, pady=5)
        self.status_text_area = scrolledtext.ScrolledText(status_frame, height=8, state=tk.DISABLED, wrap=tk.WORD)
        self.status_text_area.pack(fill=tk.BOTH, expand=True)

        # Log/Raw Serial Monitor Frame
        log_frame = ttk.LabelFrame(right_pane, text="Raw Serial Log", padding="10")
        log_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        self.log_text_area = scrolledtext.ScrolledText(log_frame, height=15, state=tk.DISABLED, wrap=tk.WORD, bg="black", fg="lightgreen")
        self.log_text_area.pack(fill=tk.BOTH, expand=True)

        self.root.after(100, self.process_serial_queue)

    def populate_serial_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combobox['values'] = ports
        if ports:
            self.port_combobox.current(0)

    def toggle_serial_connection(self):
        if not self.is_connected:
            port_name = self.port_combobox.get()
            if not port_name:
                messagebox.showerror("Connection Error", "No serial port selected.")
                return
            try:
                self.serial_port = serial.Serial(port_name, 115200, timeout=0.1) # Timeout for read
                self.is_connected = True
                self.connection_status_label.config(text=f"Status: Connected to {port_name}", foreground="green")
                self.connect_button.config(text="Disconnect")
                self.log_message(f"INFO: Connected to {port_name}\n", "info")
                self.enable_controls(True)
                
                # Start serial reading thread
                self.serial_thread = threading.Thread(target=self.read_from_serial, daemon=True)
                self.serial_thread.start()
                
                self.refresh_pokemon_list() # Automatically refresh list on connect
            except serial.SerialException as e:
                messagebox.showerror("Connection Error", f"Failed to connect to {port_name}:\n{e}")
                self.is_connected = False
        else:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            self.is_connected = False
            self.connection_status_label.config(text="Status: Not Connected", foreground="red")
            self.connect_button.config(text="Connect")
            self.log_message("INFO: Disconnected\n", "info")
            self.enable_controls(False)

    def enable_controls(self, enable):
        state = tk.NORMAL if enable else tk.DISABLED
        self.refresh_button.config(state=state)
        self.select_trade_button.config(state=state if self.selected_pokemon_listbox_idx is not None else tk.DISABLED)
        self.initiate_trade_button.config(state=state if self.selected_pokemon_for_trade is not None else tk.DISABLED)
        self.get_status_button.config(state=state)

    def send_serial_command(self, command):
        if self.is_connected and self.serial_port:
            try:
                full_command = command + "\n"
                self.serial_port.write(full_command.encode('utf-8'))
                self.log_message(f"SEND: {full_command}", "send")
            except serial.SerialException as e:
                self.log_message(f"ERROR: Failed to send command '{command}': {e}\n", "error")
                messagebox.showerror("Serial Error", f"Failed to send command: {e}")
                self.toggle_serial_connection() # Attempt to disconnect on error
        else:
            self.log_message(f"ERROR: Cannot send '{command}'. Not connected.\n", "error")
            # messagebox.showwarning("Not Connected", "Serial port is not connected.")

    def read_from_serial(self):
        while self.is_connected and self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        self.serial_queue.put(line)
            except serial.SerialException as e:
                self.log_message(f"ERROR: Serial reading error: {e}\n", "error")
                # Potentially trigger disconnect from this thread if port is lost
                self.serial_queue.put(None) # Signal error to main thread
                break 
            except Exception as e: # Catch other potential errors during decode or queue put
                self.log_message(f"ERROR: Exception in serial reader: {e}\n", "error")
            time.sleep(0.05) # Small delay to prevent busy-looping

    def process_serial_queue(self):
        try:
            while not self.serial_queue.empty():
                line = self.serial_queue.get_nowait()
                if line is None: # Error signal from reader thread
                    if self.is_connected: # Avoid multiple disconnect calls
                        messagebox.showerror("Serial Error", "Serial connection lost.")
                        self.toggle_serial_connection() # Disconnect
                    return

                self.log_message(f"RECV: {line}\n", "recv")
                self.parse_and_handle_response(line)
        except queue.Empty:
            pass
        finally:
            self.root.after(100, self.process_serial_queue) # Poll queue periodically


    def on_pokemon_list_select(self, event):
        widget = event.widget
        selection = widget.curselection()
        if selection:
            self.selected_pokemon_listbox_idx = selection[0]
            # Extract storage_index from the listbox item text (assuming format includes "Index: X")
            selected_text = widget.get(self.selected_pokemon_listbox_idx)
            try:
                # Example: "PIKA (Lvl: 25, Index: 0)"
                index_str = selected_text.split("Index: ")[1].split(")")[0]
                # self.selected_pokemon_index_for_trade = int(index_str) # This is for TRADE, not details.
                # Details view doesn't use this global var, it's passed directly.
            except IndexError:
                print(f"Could not parse index from: {selected_text}")
            
            self.enable_controls(self.is_connected) # Re-evaluate select_trade_button state
        else:
            self.selected_pokemon_listbox_idx = None
            self.enable_controls(self.is_connected)

    def refresh_pokemon_list(self):
        self.pokemon_listbox.delete(0, tk.END) # Clear listbox before refresh
        self.send_serial_command("LIST_POKEMON")

    def select_pokemon_for_trade_action(self):
        if self.selected_pokemon_listbox_idx is None:
            messagebox.showinfo("Select Pokémon", "Please select a Pokémon from the list first.")
            return
        
        selected_text = self.pokemon_listbox.get(self.selected_pokemon_listbox_idx)
        try:
            index_str = selected_text.split("Index: ")[1].split(")")[0]
            self.selected_pokemon_index_for_trade = int(index_str)
            self.send_serial_command(f"SELECT_POKEMON {self.selected_pokemon_index_for_trade}")
            self.log_message(f"INFO: Marked Pokémon at storage index {self.selected_pokemon_index_for_trade} for trade.\n", "info")
            self.initiate_trade_button.config(state=tk.NORMAL if self.is_connected else tk.DISABLED)
        except (IndexError, ValueError) as e:
            messagebox.showerror("Error", f"Could not parse Pokémon index from list: {e}")
            self.selected_pokemon_index_for_trade = None
            self.initiate_trade_button.config(state=tk.DISABLED)


    def initiate_trade_action(self):
        if self.selected_pokemon_index_for_trade is None:
            messagebox.showinfo("Initiate Trade", "No Pokémon has been designated for trade with 'Select This Pokémon for Trade'.")
            return
        self.send_serial_command("INITIATE_TRADE") # Can add MASTER/SLAVE if needed

    def get_status_action(self):
        self.send_serial_command("GET_STATUS")

    def parse_and_handle_response(self, line):
        parts = line.split(" ", 1) # Split command from args
        command = parts[0]
        args = parts[1] if len(parts) > 1 else ""

        if command == "POKEMON_LIST_START":
            self.pokemon_listbox.delete(0, tk.END) # Clear previous list
            self.log_message("INFO: Receiving Pokémon list...\n", "info")
        elif command == "POKEMON":
            # POKEMON <index> <name> <species_id>
            try:
                p_parts = args.split(" ", 2)
                idx = int(p_parts[0])
                name = p_parts[1]
                species_id_lvl_part = p_parts[2] # e.g. "25 (Lvl: 50)" - need to adapt if format changes
                
                # This parsing is brittle, depends on RP2040's exact POKEMON print format
                # Assuming format is "POKEMON <index> <nickname_or_species> <species_id>"
                # And that nickname_or_species does not contain spaces or is handled.
                # The current RP2040 serial_protocol.c sends:
                // POKEMON %d %s %u\n", i, name_to_print, pkm->main_data.species_id
                // name_to_print can be "SPECIES_ID_25"
                # For now, assume name is single word, species_id is the last part.
                
                species_id_str = p_parts[2] # Simpler parsing for now
                
                # A more robust way to get species_id if name can have spaces:
                # Find last space
                last_space = args.rfind(' ')
                species_id_str = args[last_space+1:]
                name_and_idx_parts = args[:last_space].split(' ', 1)
                # idx_str = name_and_idx_parts[0]
                # name_str = name_and_idx_parts[1]

                self.pokemon_listbox.insert(tk.END, f"{name} (Species ID: {species_id_str}, Index: {idx})")
            except Exception as e:
                self.log_message(f"ERROR: Could not parse POKEMON line: '{line}', Error: {e}\n", "error")
        elif command == "POKEMON_LIST_END":
            self.log_message("INFO: Pokémon list finished.\n", "info")
            if self.pokemon_listbox.size() == 0:
                 self.pokemon_listbox.insert(tk.END, "No Pokémon found in storage.")
        elif command == "ACK_SELECT":
            self.update_status_display(f"Acknowledged: Pokémon at index {args} selected for trade.")
        elif command == "ACK_INITIATE":
            self.update_status_display(f"Acknowledged: Trade initiated (Role: {args}).")
        elif command == "ACK_CANCEL":
            self.update_status_display("Acknowledged: Trade cancelled/reset on RP2040.")
        elif command == "STATUS":
            self.update_status_display(f"Device Status: {args}")
        elif command == "ERROR":
            self.update_status_display(f"RP2040 Error: {args}", error=True)
        elif command == "INFO": # General info from RP2040
            self.update_status_display(f"RP2040 Info: {args}")
        else:
            # If it's not a known command, treat as general status/log from RP2040
            self.update_status_display(f"RP2040: {line}")


    def update_status_display(self, message, error=False):
        self.status_text_area.config(state=tk.NORMAL)
        timestamp = time.strftime("%H:%M:%S", time.localtime())
        formatted_message = f"[{timestamp}] {message}\n"
        
        tag = "error_tag" if error else "info_tag"
        self.status_text_area.tag_configure("error_tag", foreground="red")
        self.status_text_area.tag_configure("info_tag", foreground="blue")
        
        self.status_text_area.insert(tk.END, formatted_message, tag)
        self.status_text_area.see(tk.END) # Scroll to bottom
        self.status_text_area.config(state=tk.DISABLED)

    def log_message(self, message, type="info"): # types: info, send, recv, error
        self.log_text_area.config(state=tk.NORMAL)
        timestamp = time.strftime("%H:%M:%S", time.localtime())
        prefix = ""
        tag = "default_tag"

        if type == "send":
            prefix = "[SEND] "
            tag = "send_tag"
            self.log_text_area.tag_configure("send_tag", foreground="orange")
        elif type == "recv":
            prefix = "[RECV] "
            tag = "recv_tag"
            self.log_text_area.tag_configure("recv_tag", foreground="lightgreen")
        elif type == "error":
            prefix = "[ERROR] "
            tag = "error_log_tag"
            self.log_text_area.tag_configure("error_log_tag", foreground="red")
        elif type == "info":
            prefix = "[INFO] "
            tag = "info_log_tag"
            self.log_text_area.tag_configure("info_log_tag", foreground="cyan")
        
        self.log_text_area.insert(tk.END, f"[{timestamp}] {prefix}{message}", tag)
        self.log_text_area.see(tk.END) # Scroll to bottom
        self.log_text_area.config(state=tk.DISABLED)

    def on_closing(self):
        if self.is_connected:
            self.toggle_serial_connection() # Disconnect gracefully
        self.root.destroy()

if __name__ == '__main__':
    root = tk.Tk()
    app = PokemonTraderApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()
