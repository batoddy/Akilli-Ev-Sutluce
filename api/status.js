import { createClient } from '@supabase/supabase-js';

const supabaseUrl = process.env.SUPABASE_URL || process.env.NEXT_PUBLIC_SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_SERVICE_ROLE_KEY || process.env.SUPABASE_ANON_KEY || process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY;
const supabase = createClient(supabaseUrl, supabaseKey);

export default async function handler(req, res) {
    if (req.method !== 'GET') return res.status(405).json({ error: 'Sadece GET metodu desteklenir.' });

    try {
        // Cihaz durumları tablosundaki tüm verileri çek
        const { data, error } = await supabase.from('cihaz_durumlari').select('*');
        if (error) throw error;

        // Veriyi arayüzün kolay okuyacağı formata çevir
        const cihazlar = {};
        data.forEach(row => {
            cihazlar[row.cihaz_id] = {
                komut: row.son_komut,
                tarih: row.son_guncelleme
            };
        });

        return res.status(200).json(cihazlar);
    } catch (error) {
        console.error('Okuma hatası:', error);
        return res.status(500).json({ error: 'Veriler çekilemedi.' });
    }
}