for (let year = 1970; year <= 2100; ++year) {
    for (let month = 1; month <= 12; ++month) {
        let date = `${year}-${month.toString().padStart(2, '0')}-01`;
        console.log(`    ${new Date(date).getTime() / 1000}, // ${date}`);
    }
}
