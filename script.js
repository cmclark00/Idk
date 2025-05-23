document.addEventListener('DOMContentLoaded', () => {
    const pokemonListElement = document.getElementById('pokemon-list');
    const selectedPokemonDetailsElement = document.getElementById('selected-pokemon-details');
    const initiateTradeButton = document.getElementById('initiate-trade-button');
    const tradeStatusElement = document.getElementById('trade-status');
    const noPokemonMessageElement = document.getElementById('no-pokemon-message');

    let storedPokemon = [];
    let selectedPokemonForTrade = null; // Stores the storage_index of the selected Pokemon
    let selectedPokemonForDetails = null; // Stores the storage_index for details view
    let tradeStatusInterval = null;

    const API_BASE_URL = '/api'; // Assuming the backend is served from the same origin

    // --- Fetch and Display Pokémon List ---
    async function fetchPokemonList() {
        try {
            const response = await fetch(`${API_BASE_URL}/pokemon`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            storedPokemon = await response.json();
            renderPokemonList();
        } catch (error) {
            console.error('Error fetching Pokemon list:', error);
            pokemonListElement.innerHTML = '<li>Error loading Pokémon.</li>';
        }
    }

    function renderPokemonList() {
        pokemonListElement.innerHTML = ''; // Clear existing list

        if (storedPokemon.length === 0) {
            noPokemonMessageElement.style.display = 'block';
            pokemonListElement.style.display = 'none';
        } else {
            noPokemonMessageElement.style.display = 'none';
            pokemonListElement.style.display = 'block';
            storedPokemon.forEach(pokemon => {
                const listItem = document.createElement('li');
                
                const nameSpan = document.createElement('span');
                nameSpan.textContent = `${pokemon.nickname || `Species ID: ${pokemon.species_id}`} (Lvl: ${pokemon.level}, Index: ${pokemon.storage_index})`;
                nameSpan.className = 'pokemon-item-name';
                listItem.appendChild(nameSpan);

                // Button to select for details view
                const detailsButton = document.createElement('button');
                detailsButton.textContent = 'View Details';
                detailsButton.className = 'details-pokemon-btn';
                detailsButton.addEventListener('click', (event) => {
                    event.stopPropagation(); // Prevent li click event
                    selectPokemonForDetailsView(pokemon.storage_index);
                });
                listItem.appendChild(detailsButton);

                // Button to select for trade
                const tradeSelectButton = document.createElement('button');
                tradeSelectButton.textContent = 'Select for Trade';
                tradeSelectButton.className = 'select-pokemon-btn';
                if (selectedPokemonForTrade === pokemon.storage_index) {
                    tradeSelectButton.classList.add('selected-indicator');
                    tradeSelectButton.textContent = 'Selected for Trade';
                }
                tradeSelectButton.addEventListener('click', (event) => {
                    event.stopPropagation();
                    selectPokemonForTrading(pokemon.storage_index, tradeSelectButton);
                });
                listItem.appendChild(tradeSelectButton);
                
                pokemonListElement.appendChild(listItem);
            });
        }
    }
    
    async function selectPokemonForDetailsView(storageIndex) {
        selectedPokemonForDetails = storageIndex;
        // Highlight selected item in the list (optional)
        document.querySelectorAll('#pokemon-list li').forEach(li => li.classList.remove('selected-details'));
        const targetLi = Array.from(document.querySelectorAll('#pokemon-list li'))
            .find(li => li.textContent.includes(`Index: ${storageIndex}`));
        if (targetLi) {
            targetLi.classList.add('selected-details');
        }
        
        fetchAndDisplayPokemonDetails(storageIndex);
    }


    function selectPokemonForTrading(storageIndex, buttonElement) {
        // Deselect previous button
        const allTradeButtons = document.querySelectorAll('.select-pokemon-btn');
        allTradeButtons.forEach(btn => {
            btn.classList.remove('selected-indicator');
            btn.textContent = 'Select for Trade';
        });

        if (selectedPokemonForTrade === storageIndex) {
            // Deselect if clicking the already selected one
            selectedPokemonForTrade = null;
            initiateTradeButton.disabled = true;
        } else {
            selectedPokemonForTrade = storageIndex;
            buttonElement.classList.add('selected-indicator');
            buttonElement.textContent = 'Selected for Trade';
            initiateTradeButton.disabled = false;
            console.log(`Pokemon at index ${selectedPokemonForTrade} selected for trade.`);
        }
    }

    // --- Fetch and Display Single Pokémon Details ---
    async function fetchAndDisplayPokemonDetails(storageIndex) {
        if (storageIndex === null) {
            selectedPokemonDetailsElement.innerHTML = '<p>No Pokémon selected for details.</p>';
            return;
        }
        try {
            const response = await fetch(`${API_BASE_URL}/pokemon/${storageIndex}`);
            if (!response.ok) {
                if (response.status === 404) {
                     selectedPokemonDetailsElement.innerHTML = `<p>Pokémon at index ${storageIndex} not found.</p>`;
                } else {
                    selectedPokemonDetailsElement.innerHTML = `<p>Error fetching details: ${response.statusText}</p>`;
                }
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const details = await response.json();
            renderPokemonDetails(details);
        } catch (error) {
            console.error(`Error fetching details for Pokemon at index ${storageIndex}:`, error);
            selectedPokemonDetailsElement.innerHTML = `<p>Error loading details for Pokémon at index ${storageIndex}.</p>`;
        }
    }

    function renderPokemonDetails(data) {
        if (!data || !data.pokemon_data) {
            selectedPokemonDetailsElement.innerHTML = '<p>Could not load details.</p>';
            return;
        }
        const pkm = data.pokemon_data;
        selectedPokemonDetailsElement.innerHTML = `
            <h3>${data.nickname || 'Unknown Nickname'} (OT: ${data.ot_name || 'N/A'})</h3>
            <p><strong>Storage Index:</strong> ${data.storage_index}</p>
            <p><strong>Species ID:</strong> ${pkm.species_id}</p>
            <p><strong>Level:</strong> ${pkm.level}</p>
            <p><strong>HP:</strong> ${pkm.current_hp} / ${pkm.max_hp}</p>
            <p><strong>Attack:</strong> ${pkm.attack}</p>
            <p><strong>Defense:</strong> ${pkm.defense}</p>
            <p><strong>Speed:</strong> ${pkm.speed}</p>
            <p><strong>Special:</strong> ${pkm.special}</p>
            <p><strong>Status:</strong> ${pkm.status_condition === 0 ? 'OK' : pkm.status_condition}</p>
            <p><strong>Type 1:</strong> ${pkm.type1}, <strong>Type 2:</strong> ${pkm.type2}</p>
            <p><strong>Moves:</strong> ${pkm.move1_id}, ${pkm.move2_id}, ${pkm.move3_id}, ${pkm.move4_id}</p>
            <p><strong>PP:</strong> ${pkm.move1_pp}, ${pkm.move2_pp}, ${pkm.move3_pp}, ${pkm.move4_pp}</p>
            <p><strong>OT ID:</strong> ${pkm.original_trainer_id}</p>
            <p><strong>IV Info (raw):</strong> ${pkm.iv_data}</p>
        `;
    }

    // --- Trade Operations ---
    async function initiateTrade() {
        if (selectedPokemonForTrade === null) {
            alert('Please select a Pokémon to trade first.');
            return;
        }
        initiateTradeButton.disabled = true; // Disable while attempting trade
        tradeStatusElement.innerHTML = `<p>Initiating trade for Pokémon at index ${selectedPokemonForTrade}...</p>`;

        try {
            // First, call /api/trade/select
            const selectResponse = await fetch(`${API_BASE_URL}/trade/select`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ storage_index: selectedPokemonForTrade })
            });

            if (!selectResponse.ok) {
                const errorResult = await selectResponse.json().catch(() => ({}));
                throw new Error(`Failed to select Pokémon for trade: ${selectResponse.status} ${selectResponse.statusText}. ${errorResult.error || ''}`);
            }
            const selectResult = await selectResponse.json();
            console.log('Select API call result:', selectResult.message);


            // Then, call /api/trade/start
            const startResponse = await fetch(`${API_BASE_URL}/trade/start`, {
                method: 'POST',
                // No body needed if selection is handled by the previous call
            });

            if (!startResponse.ok) {
                const errorResult = await startResponse.json().catch(() => ({}));
                throw new Error(`Failed to start trade: ${startResponse.status} ${startResponse.statusText}. ${errorResult.error || ''}`);
            }
            const startResult = await startResponse.json();
            updateTradeStatusDisplay(startResult.status_code || 'STARTED', startResult.message);

            if (tradeStatusInterval) clearInterval(tradeStatusInterval);
            tradeStatusInterval = setInterval(fetchTradeStatus, 2000); // Poll every 2 seconds
        } catch (error) {
            console.error('Error initiating trade:', error);
            tradeStatusElement.innerHTML = `<p>Error: ${error.message}</p>`;
            initiateTradeButton.disabled = false; // Re-enable if setup failed
        }
    }

    async function fetchTradeStatus() {
        try {
            const response = await fetch(`${API_BASE_URL}/trade/status`);
            if (!response.ok) {
                // If status endpoint itself fails, show a generic error but keep polling for a bit
                console.error(`Trade status HTTP error! status: ${response.status}`);
                updateTradeStatusDisplay('ERROR', `Error fetching status: ${response.statusText}. Retrying...`);
                return; 
            }
            const statusData = await response.json();
            updateTradeStatusDisplay(statusData.status_code, statusData.status_message, statusData);

            // Stop polling if trade is complete, failed, or idle after an attempt
            if (statusData.status_code === 'TRADE_COMPLETE' || statusData.status_code === 'TRADE_FAILED' || statusData.status_code === 'TRADE_CANCELLED' || (statusData.status_code === 'IDLE' && statusData.trade_id !== "null")) {
                clearInterval(tradeStatusInterval);
                tradeStatusInterval = null;
                initiateTradeButton.disabled = false; // Re-enable button
                fetchPokemonList(); // Refresh Pokemon list in case one was traded away/received
                if (statusData.status_code === 'TRADE_COMPLETE' && statusData.received_pokemon_summary) {
                    alert(`Trade complete! Received ${statusData.received_pokemon_summary.nickname || 'a Pokemon'}.`);
                }
            }
        } catch (error) {
            console.error('Error fetching trade status:', error);
            updateTradeStatusDisplay('ERROR', 'Failed to fetch trade status. Retrying...');
            // Potentially stop polling after too many errors if desired
        }
    }

    function updateTradeStatusDisplay(statusCode, message, fullStatusData = null) {
        let html = `<p><strong>Status:</strong> ${statusCode || 'N/A'}</p><p>${message || 'No message.'}</p>`;
        if (fullStatusData) {
            if (fullStatusData.offered_pokemon_index !== undefined && fullStatusData.offered_pokemon_index !== -1) {
                html += `<p>Offering Pokémon at index: ${fullStatusData.offered_pokemon_index}</p>`;
            }
            if (fullStatusData.received_pokemon_summary) {
                const received = fullStatusData.received_pokemon_summary;
                html += `<p>Partner offered: ${received.nickname || `Species ID ${received.species_id}`} (Lvl: ${received.level})</p>`;
                 if (statusCode === 'TRADE_COMPLETE' && received.new_storage_index !== undefined) {
                    html += `<p>Received Pokémon stored at index: ${received.new_storage_index}</p>`;
                }
            }
        }
        tradeStatusElement.innerHTML = html;
    }

    // --- Event Listeners ---
    initiateTradeButton.addEventListener('click', initiateTrade);

    // --- Initial Load ---
    fetchPokemonList();
    updateTradeStatusDisplay('IDLE', 'System ready. Select a Pokémon and initiate trade.'); // Initial status
});
