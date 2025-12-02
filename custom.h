const char* const customJS = R"(
// redraw the label as a table and a canvas.
// the table has two rows. the top row are the bit values
// and the bottom row is the second within the minute.
// the canvas shows the logical high/low pin values.
/**
 * Converts a span's text into a visualization containing a Canvas diagram and a Data Table.
 * @param {HTMLElement} containerSpan - The target DOM element.
 */
function convertToTable(containerSpan) {
    const rawText = containerSpan.textContent;
    const characters = Array.from(rawText);

    // --- Configuration ---
    const CANVAS_HEIGHT = 20;
    const BOX_HEIGHT = 2;
    // Maps characters to the percentage of width allocated to the "left" (short) box.
    // 'M': 80% short, 20% tall. '0': 20% short, 80% tall.
    const DRAW_RATIOS = { 'M': 0.8, '0': 0.2, '1': 0.5 };
    const margin = 2; // 2px margin around the highlight content
    const highlightsConfig = [
        { label: "Minutes", start: 1, end: 8 },
        { label: "Hours", start: 12, end: 18 },
        { label: "Day of year", start: 22, end: 33 },
        { label: "Year", start: 45, end: 53 },
    ];



    // --- 1. Setup Container & Generate Table ---
    containerSpan.classList.add('visualized-container');
    containerSpan.style.display = 'flex';
    containerSpan.style.flexDirection = 'column';

    // Create Table HTML efficiently
    const dataCells = characters.map(char => `<td>${char}</td>`).join('');
    const indexCells = characters.map((_, i) => `<th>${String(i).padStart(2, '0')}</th>`).join('');

    const tableHTML = `
        <table class="char-table">
            <tbody><tr>${dataCells}</tr></tbody>
            <tfoot><tr>${indexCells}</tr></tfoot>
        </table>
    `;

    // Insert the table immediately. 
    containerSpan.innerHTML = tableHTML;
    const table = containerSpan.firstElementChild;

    // --- 2. Measure Dimensions ---
    // We map over the cells to get precise pixel measurements based on CSS rendering
    const cells = Array.from(table.querySelectorAll('tbody td'));
    const layoutMap = cells.map((cell, index) => ({
        char: characters[index],
        left: cell.offsetLeft,
        width: cell.offsetWidth
    }));

    // --- 3. Configure Canvas ---
    const canvas = document.createElement('canvas');
    canvas.width = table.offsetWidth;
    canvas.height = CANVAS_HEIGHT;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#10B981'; // Emerald Green

    // --- 4. Draw ---
    const baseLine = CANVAS_HEIGHT;

    layoutMap.forEach(({ char, left, width }) => {
        const ratio = DRAW_RATIOS[char];
        
        // If character is not in our config (e.g., spaces), skip drawing
        if (ratio === undefined) return;

        // Calculate split point (floor box vs tall box)
        const splitWidth = Math.round(width * ratio);
        const remainingWidth = width - splitWidth;

        // Box 1: The short "floor" box (Left side)
        ctx.fillRect(left, baseLine - BOX_HEIGHT, splitWidth, BOX_HEIGHT);

        // Box 2: The tall "wall" box (Right side)
        ctx.fillRect(left + splitWidth, 0, remainingWidth, CANVAS_HEIGHT);
    });

    // --- 5. Outline Creation (Loop for Multiple Ranges) ---
    const highlightElements = [];
    
    // Set tbody to relative positioning context for all highlight elements
    const tbody = table.querySelector('tbody');
    tbody.style.position = 'relative'; 

    highlightsConfig.forEach(config => {
        const { label, start, end } = config;

        const startCellDim = layoutMap[start];
        const endCellDim = layoutMap[end];
        
        const highlightRawEnd = endCellDim.left + endCellDim.width;
        const highlightRawStart = startCellDim.left;
        const totalRawWidth = highlightRawEnd - highlightRawStart;

        // 1. Width: Total span of cells minus 2*margin.
        const highlightWidth = Math.round(totalRawWidth - (margin * 2));

        // 2. Left Position (Relative to tbody/table): Start of cell + 2px margin
        const highlightLeft = Math.round(highlightRawStart + margin);

        // Create the highlight element
        const highlightDiv = document.createElement('div');
        highlightDiv.className = 'highlight-overlay';
        highlightDiv.textContent = label;
        highlightDiv.style.left = `${highlightLeft}px`;
        highlightDiv.style.width = `${highlightWidth}px`;

        highlightElements.push(highlightDiv);
    });

    // --- 6. Final Placement ---
    // This keeps the table in the DOM (preserving state/layout) and adds the canvas above it.
    containerSpan.prepend(canvas);
    highlightElements.forEach(highlight => tbody.appendChild(highlight));
}

// re-draw the label every time it is updated
const masterObserver = new MutationObserver((mutations) => {
    const label = document.getElementById('l1');
    if (!label) return;

    // If it contains the table we added, ignore this update (it was us!)
    if (label.querySelector('canvas')) return;    

    convertToTable(label);
});

// Start observing the entire DOM
// (we don't observe just the label element due to the zombie node problem.
// it works most of the time, but if the browser falls behind eg. while the
// computer is asleep, it may replace the element on waking up which breaks
// the observer.)
window.addEventListener('load', function() {
    masterObserver.observe(document.body, {
        childList: true,    // Detects if #l1 is added/removed
        subtree: true,      // Detects changes deep inside the DOM
        characterData: true // Detects text changes inside existing nodes
    });
});
)";



const char* const customCSS = R"(
        #id1 td {
            border: 1px solid #ccc;
            padding: 4px 6px;
            font-family: monospace;
            text-align: center;
            background-color: #f9f9f9; 
            color: #864D0F;
        }
        #id1 {
            border-collapse: collapse;
            /* Ensure table fits content when placed directly in the span */
            width: auto; 
        }
        /* Style for the index row */
        #id1 tfoot th {
            background-color: #e0f2f1; /* Light cyan background for index row */
            color: #00796b;
            font-weight: bold;
            text-align: center;
        }
        #id1 canvas {
            margin-top: 5px;
            margin-bottom: 3px;
        }
        /* Style to ensure the original span is a block container for the table/canvas */
        .visualized-container {
            display: flex;
            flex-direction: column;
            align-items: flex-start; /* Aligns content to the left for correct scrolling */
            width: 100%;
            padding: 0;
            border: none;
            background: none;
            text-align: left;
            /* Ensure it allows horizontal scrolling */
            overflow-x: auto; 
        }
        .highlight-overlay {
            --highlight-top-margin: 4px;
            --highlight-bottom-margin: 4px;
            background-color: transparent; /* No fill */
            border: 2px solid rgba(255, 193, 7, 0.9); /* Opaque yellow outline */
            color: rgba(255, 193, 7, 0.9);
            pointer-events: none; /* Allows interaction with elements beneath it */
            z-index: 5; 
            position: absolute; /* Positioned relative to the table body */
            box-sizing: border-box; /* Width/Height include the border thickness */
            top: 0px;
            margin-top: var(--highlight-top-margin);
            height: calc(100% - var(--highlight-top-margin) - var(--highlight-bottom-margin));
        }
        #id1 tr td {
            height: 40px;
            vertical-align: bottom;
        }
)";