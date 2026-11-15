export function addSolarAlert() {
    const alert = document.getElementById('solar-alert');
    const title = document.getElementById('solar-title');
    const details = document.getElementById('solar-details');
    const button = document.getElementById('solar-demo');

    function updateSolar(event) {
        if (event === null) {
            alert.hidden = true;
            return;
        }

        if (!['forecast', 'active'].includes(event.phase)) {
            throw new Error('Unknown solar event phase');
        }

        const prefix = event.simulated ? 'SIMULATION · ' : '';
        alert.dataset.phase = event.phase;

        if (event.phase === 'forecast') {
            title.textContent = `${prefix}Solar particle event watch`;
            const minutes = event.estimatedArrivalMinutes;
            const arrival = Number.isFinite(minutes) && minutes >= 0
                ? `Estimated onset in ${Math.round(minutes)} min.`
                : 'Arrival time uncertain.';
            details.textContent = `${arrival} Review EVA timing and shelter access.`;
        } else {
            title.textContent = `${prefix}Solar particle event active`;
            details.textContent = 'Elevated radiation alert for Gale. Check crew shelter status.';
        }

        alert.hidden = false;
    }

    let phase = 'clear';

    function getSolarState() {
        if (phase === 'active') {
            return { phase, speActive: true, speRate: 350.0, speMultiplier: 12.0, speOnset: 0.0 };
        }
        if (phase === 'forecast') {
            return { phase, speActive: true, speRate: 180.0, speMultiplier: 6.0, speOnset: 120.0 };
        }
        return { phase: 'clear', speActive: false, speRate: 0.0, speMultiplier: 1.0, speOnset: 0.0 };
    }

    button.addEventListener('click', () => {
        if (phase === 'clear') {
            phase = 'forecast';
            updateSolar({ phase, estimatedArrivalMinutes: 10, simulated: true });
            button.textContent = 'Simulate event onset';
        } else if (phase === 'forecast') {
            phase = 'active';
            updateSolar({ phase, simulated: true });
            button.textContent = 'End solar simulation';
        } else {
            phase = 'clear';
            updateSolar(null);
            button.textContent = 'Simulate solar event';
        }
        window.dispatchEvent(new CustomEvent('solar-change', { detail: getSolarState() }));
    });

    return { updateSolar, getSolarState };
}   