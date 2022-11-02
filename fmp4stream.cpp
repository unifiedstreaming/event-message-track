/*******************************************************************************
Supplementary software media ingest specification:

https://github.com/unifiedstreaming/fmp4-ingest

Copyright (C) 2009-2021 CodeShop B.V.
http://www.code-shop.com

******************************************************************************/

#include <stdint.h>
#include <iostream>
#include <sstream>
#include <iomanip> 
#include <memory>
#include <limits>
#include <string> 
#include "fmp4stream.h"
#include "base64.h"


/*------------------------ -

warning fmp4 functionality is only partially implemented 
for better results use exising libraries/parsers

------------------*/

#include <stdint.h>



namespace fmp4_stream
{
    

	void box::parse(char const* ptr)
	{
		size_ = fmp4_read_uint32(ptr);
		box_type_ = std::string(ptr + 4, ptr + 8);
		if (size_ == 1) {
			large_size_ = fmp4_read_uint64(ptr + 8);
		}
	}

	// only the size of the beginning of the box structure
	uint64_t box::size()  const {
		uint64_t l_size = 8;
		if (is_large_)
			l_size += 8;
		if (has_uuid_)
			l_size += 16;
		return l_size;
	};

	void box::print() const
	{
		std::cout << "=================" << box_type_ << "==================" << std::endl;
		std::cout << std::setw(33) << std::left << " box size: " << size_ << std::endl;
	}

	// only partially implemented to parse timescale
	void mvhd::parse(char const *ptr)
	{
		this->version_ = (unsigned int)ptr[8];

		if (version_ == 1)
			this->time_scale_ = fmp4_read_uint32(ptr + 12 + 8 + 8);
		else
			this->time_scale_ = fmp4_read_uint32(ptr + 12 + 8);

		return;
	}

	bool box::read(std::istream &istr)
	{
		box_data_.resize(9);
		large_size_ = 0;

		istr.read((char*)&box_data_[0], 8);
		size_ = fmp4_read_uint32((char *)&box_data_[0]);
		box_data_[8] = (uint8_t) '\0';
		box_type_ = std::string((char *)&box_data_[4]);
		if (box_type_.compare("uuid") == 0)
			has_uuid_ = true;

		// detect large size box 
		if (size_ == 1)
		{
			is_large_ = true;
			box_data_.resize(16);
			istr.read((char *)&box_data_[8], 8);
			large_size_ = fmp4_read_uint64((char *)&box_data_[8]);
		}
		else 
		{
			large_size_ = size_;
			box_data_.resize(8);
		}

		// read all the bytes from the stream
		if (size_) 
		{
			// write the offset bytes of the box
			uint8_t offset = (uint8_t)box_data_.size();
			box_data_.resize(large_size_);
			if ((large_size_ - offset) > 0)
				istr.read((char*)&box_data_[offset], large_size_ - offset);
			return true;
		}

		return false;
	};

	void full_box::parse(char const *ptr)
	{
		box::parse(ptr);
		// read the version and flags
		uint64_t offset = box::size();
		magic_conf_ = fmp4_read_uint32(ptr + offset);
		this->version_ = *((const uint8_t *)((ptr + offset)));
		this->flags_ = ((uint32_t)(0x00FFFFFF)) & fmp4_read_uint32(ptr + offset);
	}

	void full_box::print() const
	{
		box::print();
		std::cout << std::setw(33) << std::left << "version: " << this->version_ << std::endl;
		std::cout << std::setw(33) << std::left << "flags: " << flags_ << std::endl;
	}

	void mfhd::parse(char const * ptr)
	{
		seq_nr_ = fmp4_read_uint32(ptr + 12);
	}

	void mfhd::print() const
	{
		std::cout << "=================mfhd==================" << std::endl;
		std::cout << std::setw(33) << std::left << " sequence number is: " << seq_nr_ << std::endl;
	};

	void tfhd::parse(const char * ptr)
	{
		full_box::parse(ptr);
		track_id_ = fmp4_read_uint32(ptr + 12);
		base_data_offset_present_ = !! (0x1u) & flags_;
		sample_description_index_present_ = !!(0x2u & flags_);
		default_sample_duration_present_ = !!(0x00000008u & flags_);
		default_sample_size_present_ = !!(0x00000010u & flags_);
		default_sample_flags_present_ = !!(0x00000020u & flags_);
		duration_is_empty_ = !!(0x00010000u & flags_);
		default_base_is_moof_ = !!(0x00020000u & flags_);

		unsigned int offset = 16;

		if (base_data_offset_present_)
		{
			base_data_offset_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
		}
		if (sample_description_index_present_)
		{
			sample_description_index_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
		}
		if (default_sample_duration_present_)
		{
			default_sample_duration_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
		}
		if (default_sample_size_present_)
		{
			default_sample_size_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
		}
		if (default_sample_flags_present_)
		{
			default_sample_flags_ = (uint32_t)fmp4_read_uint32(ptr + offset);
			offset += 4;
		}
	}

	uint64_t tfhd::size() const
	{
		uint64_t l_size = full_box::size() + 4;
		if (base_data_offset_present_)
			l_size += 8;
		if (sample_description_index_)
			l_size += 4;
		if (default_sample_duration_present_)
			l_size += 4;
		if (default_sample_size_present_)
			l_size += 4;
		if (default_sample_flags_present_)
			l_size += 4;

		return l_size;
	};

	void tfhd::print() const {
		std::cout << "=================tfhd==================" << std::endl;
		//cout << setw(33) << left << " magic conf                 " << m_magic_conf << endl;
		std::cout << std::setw(33) << std::left << " vflags                  " << flags_ << std::endl;
		std::cout << std::setw(33) << std::left << " track id:                  " << track_id_ << std::endl;
		if (base_data_offset_present_)
			std::cout << std::setw(33) << std::left << " base_data_offset: " << base_data_offset_ << std::endl;
		if (sample_description_index_present_)
			std::cout << std::setw(33) << std::left << " sample description: " << sample_description_index_ << std::endl;
		if (default_sample_duration_present_)
			std::cout << std::setw(33) << std::left << " default sample duration: " << default_sample_duration_ << std::endl;
		if (default_sample_size_present_)
			std::cout << std::setw(33) << std::left << " default sample size: " << default_sample_size_ << std::endl;
		if (default_sample_flags_present_)
			std::cout << std::setw(33) << std::left << " default sample flags:  " << default_sample_flags_ << std::endl;
		std::cout << std::setw(33) << std::left << " duration is empty " << duration_is_empty_ << std::endl;
		std::cout << std::setw(33) << std::left << " default base is moof" << default_base_is_moof_ << std::endl;
		//cout << "............." << std::endl;
	};

	uint64_t tfdt::size() const
	{
		return version_ ? full_box::size() + 8 : full_box::size() + 4;
	};

	void tfdt::print() const {
		std::cout << "=================tfdt==================" << std::endl;
		std::cout << std::setw(33) << std::left << " basemediadecodetime: " << base_media_decode_time_ << std::endl;
	};

	void tfdt::parse(const char* ptr)
	{
		full_box::parse(ptr);
		base_media_decode_time_ = version_ ? \
			fmp4_read_uint64((char *)(ptr + 12)) : \
			fmp4_read_uint32((char *)(ptr + 12));

	};

	uint64_t trun::size() const
	{
		uint64_t l_size = full_box::size() + 4;
		if (data_offset_present_)
			l_size += 4;
		if (first_sample_flags_present_)
			l_size += 4;
		if (sample_duration_present_)
			l_size += (sample_count_ * 4);
		if (sample_size_present_)
			l_size += (sample_count_ * 4);
		if (sample_flags_present_)
			l_size += (sample_count_ * 4);
		if (this->sample_composition_time_offsets_present_)
			l_size += (sample_count_ * 4);
		return l_size;
	};

	void trun::print() const
	{
		std::cout << "==================trun=================" << std::endl;
		std::cout << std::setw(33) << std::left << " magic conf                 " << magic_conf_ << std::endl;
		std::cout << std::setw(33) << std::left << " sample count:      " << sample_count_ << std::endl;
		if (data_offset_present_)
			std::cout << std::setw(33) << std::left << " data_offset:        " << data_offset_ << std::endl;
		if (first_sample_flags_present_)
			std::cout << " first sample flags:        " << first_sample_flags_ << std::endl;

		std::cout << std::setw(33) << std::left << " first sample flags present:  " << first_sample_flags_present_ << std::endl;
		std::cout << std::setw(33) << std::left << " sample duration present: " << sample_duration_present_ << std::endl;
		std::cout << std::setw(33) << std::left << " sample_size_present: " << sample_size_present_ << std::endl;
		std::cout << std::setw(33) << std::left << " sample_flags_present: " << sample_flags_present_ << std::endl;
		std::cout << std::setw(33) << std::left << " sample_ct_offsets_present: " << sample_composition_time_offsets_present_ << std::endl;

		for (unsigned int i = 0; i < m_sentry.size(); i++)
			m_sentry[i].print();
	};

	void trun::parse(const char * ptr)
	{
		full_box::parse(ptr);

		sample_count_ = fmp4_read_uint32(ptr + 12);

		std::bitset<32> bb(flags_);

		data_offset_present_ = bb[0];
		//cout << "data_offset_present " << data_offset_present << endl;
		first_sample_flags_present_ = bb[2];
		//cout << "first_sample_flags_present " << first_sample_flags_present << endl;
		sample_duration_present_ = bb[8];
		//cout << "sample_duration_present " << sample_duration_present << endl;
		sample_size_present_ = bb[9];
		//cout << "sample_size_present " << sample_size_present << endl;
		sample_flags_present_ = bb[10];
		sample_composition_time_offsets_present_ = bb[11];

		//sentry.resize(sample_count);
		unsigned int offset = 16;
		if (data_offset_present_)
		{
			this->data_offset_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
		}if (first_sample_flags_present_) {
			this->first_sample_flags_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
		}
		// write all the entries to the trun box
		for (unsigned int i = 0; i < sample_count_; i++)
		{
			sample_entry ent = {};
			if (sample_duration_present_)
			{
				ent.sample_duration_ = fmp4_read_uint32(ptr + offset);
				offset += 4;
			}
			if (sample_size_present_)
			{
				ent.sample_size_ = fmp4_read_uint32(ptr + offset);
				offset += 4;
			}
			if (sample_flags_present_)
			{
				ent.sample_flags_ = fmp4_read_uint32(ptr + offset);
				offset += 4;
			}
			if (sample_composition_time_offsets_present_)
			{
				if (version_ == 0)
					ent.sample_composition_time_offset_v0_ = fmp4_read_uint32(ptr + offset);
				else
					ent.sample_composition_time_offset_v1_ = fmp4_read_uint32(ptr + offset);
				offset += 4;
			}
			m_sentry.push_back(ent);
		}
	}

	uint64_t media_fragment::get_duration()
	{
		uint64_t duration = 0;

		for (unsigned int i = 0; i < trun_.sample_count_; i++)
		{
			uint32_t sample_duration = tfhd_.default_sample_duration_;
			if (trun_.sample_duration_present_)
			{
				sample_duration = trun_.m_sentry[i].sample_duration_;
			}

			duration += sample_duration;
		}
		return duration;
	}

	// warning not fully completed
	void sc35_splice_info::print(bool verbose) const
	{
		if (verbose) {
			std::cout << std::setw(33) << std::left << " table id: " << (unsigned int)table_id_ << std::endl;
			std::cout << std::setw(33) << std::left << " section_syntax_indicator: " << section_syntax_indicator_ << std::endl;
			std::cout << std::setw(33) << std::left << " private_indicator: " << private_indicator_ << std::endl;
			std::cout << std::setw(33) << std::left << " section_length: " << (unsigned int)section_length_ << std::endl;
			std::cout << std::setw(33) << std::left << " protocol_version: " << (unsigned int)protocol_version_ << std::endl;
			std::cout << std::setw(33) << std::left << " encrypted_packet: " << encrypted_packet_ << std::endl;
			std::cout << std::setw(33) << std::left << " encryption_algorithm: " << (unsigned int)encryption_algorithm_ << std::endl;
			std::cout << std::setw(33) << std::left << " pts_adjustment: " << pts_adjustment_ << std::endl;
			std::cout << std::setw(33) << std::left << " cw_index: " << (unsigned int)cw_index_ << std::endl;
			std::cout << std::setw(33) << std::left << " tier: " << tier_ << std::endl;
			std::cout << std::setw(33) << std::left << " splice_command_length: " << splice_command_length_ << std::endl;
			std::cout << std::setw(33) << std::left << " splice_command_type: " << (unsigned int)splice_command_type_ << std::endl;
			std::cout << std::setw(33) << std::left << " descriptor_loop_length: " << descriptor_loop_length_ << std::endl;
		}
		std::cout << std::setw(33) << std::left << " pts_adjustment: " << pts_adjustment_ << std::endl;
		std::cout << std::setw(33) << std::left << " Command type: ";

		switch (splice_command_type_)
		{
		case 0x00: std::cout << 0x00; // prints "1"
			std::cout << std::setw(33) << std::left << " splice null " << std::endl;
			break;       // and exits the switch
		case 0x04:
			std::cout << std::setw(33) << std::left << " splice schedule " << std::endl;
			break;
		case 0x05:
			std::cout << std::setw(33) << std::left << " splice insert " << std::endl;
			std::cout << std::setw(33) << std::left << " event_id: " << splice_insert_event_id_ << std::endl;
			std::cout << std::setw(33) << std::left << " cancel indicator: " << splice_event_cancel_indicator_ << std::endl;
			break;
		case 0x06:
			std::cout << std::setw(33) << std::left << "time signal " << std::endl;
			break;
		case 0x07:
			std::cout << std::setw(33) << std::left << "bandwidth reservation " << std::endl;
			break;
		}
	}

	// warning not fully completed
	void sc35_splice_info::parse(const uint8_t *ptr, unsigned int size)
	{
		table_id_ = *ptr++;

		if (table_id_ != 0xFC)
		{
			std::cout << " error parsing table id, tableid != 0xFC " << std::endl;
			return;
		}

		std::bitset<8> b(*ptr);
		section_syntax_indicator_ = b[7];
		private_indicator_ = b[6];
		section_length_ = 0x0FFF & fmp4_read_uint16((char *)ptr);
		ptr += 2;
		protocol_version_ = (uint8_t)*ptr++;

		b = std::bitset<8>(*ptr);

		encrypted_packet_ = b[7];
		bool pts_highest_bit = b[0];

		b[0] = 0;
		b[7] = 0;
		encryption_algorithm_ = uint8_t(b.to_ullong() >> 1);
		ptr++;

		pts_adjustment_ = (uint64_t)fmp4_read_uint32((char *)ptr);
		if (pts_highest_bit)
		{
			pts_adjustment_ += (uint64_t)std::numeric_limits<uint32_t>::max();
		}
		ptr = ptr + 4;
		cw_index_ = *ptr++;

		uint32_t wd = fmp4_read_uint32((char *)ptr);
		std::bitset<32> a(wd);
		tier_ = (wd & 0xFFF00000) >> 20;
		splice_command_length_ = (0x000FFF00 & wd) >> 8;
		splice_command_type_ = (uint8_t)(0x000000FF & wd);
		ptr += 4;
		descriptor_loop_length_ = fmp4_read_uint16((char *)ptr);

		ptr += descriptor_loop_length_;
		if (splice_command_type_ == 0x05) {
			splice_insert_event_id_ = fmp4_read_uint32((char *)ptr);
			ptr += 4;
			std::bitset<8> bb(*ptr);
			splice_event_cancel_indicator_ = bb[7];
		}
	}

	const std::string base64splice_insert("/DAhAAAAAAAAAP/wEAUAAAMrf+9//gAaF7DAAAAAAADkYSQC");

	// todo test and fix crc 32 calculation/
	/*
	uint32_t crc32_for_byte(uint32_t r) {
		for (int j = 0; j < 8; ++j)
			r = (r & 1 ? 0 : (uint32_t)0xEDB88320L) ^ r >> 1;
		return r ^ (uint32_t)0xFF000000L;
	}

	void crc32_fmp4(const void *data, size_t n_bytes, uint32_t* crc) {
		static uint32_t table[0x100];
		if (!*table)
			for (size_t i = 0; i < 0x100; ++i)
				table[i] = (uint32_t)crc32_for_byte(i);
		for (size_t i = 0; i < n_bytes; ++i)
			*crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
	}*/

	// generate a splice insert command
	void gen_splice_insert(std::vector<uint8_t> &out_splice_insert,int event_id, uint32_t duration, bool splice_immediate)
	{
		out_splice_insert = base64_decode(base64splice_insert);

		uint8_t *ptr = &out_splice_insert[0];
		if (splice_immediate)
		{
			*(ptr + 2) = *(ptr + 2) - 1;
		}

		ptr += 12;
		
		if (splice_immediate)
		{
			*(ptr ) = *(ptr) - 1;
		}

		ptr += 2;
		fmp4_write_uint32(event_id, (char *)ptr);
		ptr += 5;
		
		std::bitset<8> b2(*ptr);
		bool out_of_network_indicator = b2[7];
		bool program_splice_flag = b2[6];
		bool duration_flag = b2[5];
		bool splice_immediate_flag = b2[4];
		b2[4] = splice_immediate;
		*ptr = (unsigned char)b2.to_ulong();
		
		if (program_splice_flag && splice_immediate)
		{
			std::vector<uint8_t> trailer;
			for (int k = 21; k < out_splice_insert.size() - 1; k++)
			{
				trailer.push_back(out_splice_insert[k]);
			}
			ptr = &trailer[0];
			ptr++;
			if (duration_flag)
			{
				fmp4_write_uint32(duration, (const char*)ptr);
				ptr += 4;
			}
			for (int k = 0; k < trailer.size(); k++)
			{
				out_splice_insert[k + 20] = trailer[k];
			}
			int32_t or_size = out_splice_insert.size();
			out_splice_insert.resize(or_size - 1);
			return;
		}
		
		ptr+=3;
	
		if (duration_flag)
		{
			fmp4_write_uint32(duration, (const char *)ptr);
			ptr += 4;
		}
	
		//uint32_t crc = 0;
		//crc32_fmp4(&out_splice_insert[0], out_splice_insert.size() - 4, &crc);

		//ptr = &out_splice_insert[out_splice_insert.size() - 4];
		//fmp4_write_uint32(crc, (const char*)ptr);
		//uint32_t table[256];
		//crc::crc32::generate_table(table);

		//char* ptr2 = (char*)&out_splice_insert[0];
		//uint16_t slen = out_splice_insert.size() - 4;
		//printf("Size of Data struct is: %u\n", slen);                 // 16 bytes

		//uint32_t CRC = 0;
		//for (int cnt = 0; cnt < slen; cnt++) {
		//	CRC = crc::crc32::update(table, CRC, ptr2, 1);
		//	ptr2++;
		//}
		//// get the original crc
		//uint32_t or_crc = fmp4_read_uint32(ptr2);
		////fmp4_write_uint32(CRC, (char*)ptr2);
		//printf("Piece-wise crc32 of struct Data is: 0x%X \n", CRC);
	}


	uint64_t emsg::size() const
	{
		uint64_t l_size = full_box::size();
		if (version_ == 1)
			l_size += 4;
		l_size += 16; // presentation time duration
		l_size += scheme_id_uri_.size() + 1;
		l_size += value_.size() + 1;
		l_size += message_data_.size();
		return l_size;
	}

	void emsg::parse(const char *ptr, unsigned int data_size)
	{
		full_box::parse(ptr);
		uint64_t offset = full_box::size();
		if (version_ == 0)
		{
			scheme_id_uri_ = std::string(ptr + offset);
			//cout << "scheme_id_uri: " << scheme_id_uri << endl;
			offset = offset + scheme_id_uri_.size() + 1;
			value_ = std::string(ptr + offset);
			//cout << "value: " << value << endl;
			offset = offset + value_.size() + 1;
			timescale_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
			//cout << "timescale: " << timescale << endl;
			presentation_time_delta_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
			//cout << "presentation_time_delta: " << presentation_time_delta << endl;
			event_duration_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
			//cout << "event_duration: " << event_duration << endl;
			id_ = fmp4_read_uint32(ptr + offset);
			//cout << "id: " << id << endl;
			offset += 4;
		}
		else
		{
			timescale_ = fmp4_read_uint32(ptr + offset);
			//cout << "timescale: " << timescale << endl;
			offset += 4;
			presentation_time_ = fmp4_read_uint64(ptr + offset);
			//cout << "presentation time: " << presentation_time << endl;
			offset += 8;
			event_duration_ = fmp4_read_uint32(ptr + offset);
			//cout << "event duration: " << event_duration << endl;
			offset += 4;
			id_ = fmp4_read_uint32(ptr + offset);
			offset += 4;
			//cout << "id: " << id << endl;
			scheme_id_uri_ = std::string(ptr + offset);
			offset = offset + scheme_id_uri_.size() + 1;
			//cout << "scheme_id_uri: " << scheme_id_uri << endl;
			value_ = std::string(ptr + offset);
			//cout << "value: " << value << endl;
			offset = offset + value_.size() + 1;
		}
		for (unsigned int i = (unsigned int)offset; i < data_size; i++)
		{
			message_data_.push_back(*(ptr + (size_t)i));
		}
	}

	//! emsg to mpd event, always to base64 encoding
	void emsg::write_emsg_as_mpd_event(std::ostream &ostr, uint64_t base_time) const
	{
		ostr << "<event "
			<< "presentationtime=" << '"' << (this->version_ ? presentation_time_ : base_time + presentation_time_delta_) << '"' << " "  \
			<< "duration=" << '"' << event_duration_ << '"' << " "  \
			<< "id=" << '"' << id_ << '"';
		if (this->scheme_id_uri_.compare("urn:scte:scte35:2013:bin") == 0) // write binary scte as xml + bin as defined by scte-35
		{
			ostr << '>' << std::endl << "  <signal xmlns=" << '"' << "http://www.scte.org/schemas/35/2016" << '"' << '>' << std::endl \
				<< "    <binary>" << base64_encode(this->message_data_.data(), (unsigned int)this->message_data_.size()) << "</binary>" << std::endl
				<< "  </signal>" << std::endl;
		}
		else {
			ostr << " " << "contentencoding=" << '"' << "base64" << '"' << '>' << std::endl
				<< base64_encode(this->message_data_.data(), (unsigned int)this->message_data_.size()) << std::endl;
		}
		ostr << "</event>" << std::endl;
	}

	//!
	void emsg::print() const
	{
		std::cout << "=================emsg==================" << std::endl;
		std::cout << std::setw(33) << std::left << " e-msg box version: " << (unsigned int)version_ << std::endl;
		std::cout << " scheme_id_uri:     " << scheme_id_uri_ << std::endl;
		std::cout << std::setw(33) << std::left << " value:             " << value_ << std::endl;
		std::cout << std::setw(33) << std::left << " timescale:         " << timescale_ << std::endl;
		if (version_ == 1)
			std::cout << std::setw(33) << std::left << " presentation_time: " << presentation_time_ << std::endl;
		else
			std::cout << std::setw(33) << std::left << " presentation_time_delta: " << presentation_time_delta_ << std::endl;
		std::cout << std::setw(33) << std::left << " event duration:    " << event_duration_ << std::endl;
		std::cout << std::setw(33) << std::left << " event id           " << id_ << std::endl;
		std::cout << std::setw(33) << std::left << " message data size  " << message_data_.size() << std::endl;

		//print_payload

		if (message_data_[0] == 0xFC)
		{
			// splice table
			//cout << " scte splice info" << endl;
			sc35_splice_info l_splice;
			l_splice.parse((uint8_t*)&message_data_[0], (unsigned int)message_data_.size());
			std::cout << "=============splice info==============" << std::endl;
			l_splice.print();
		}
	}

	// write an emsg box to a stream
	uint32_t emsg::write(std::ostream &ostr) const
	{
		char int_buf[4];
		char long_buf[8];
		uint32_t bytes_written = 0;

		uint32_t sz = (uint32_t)this->size();
		fmp4_write_uint32(sz, int_buf);
		ostr.write((char *)int_buf, 4);
		bytes_written += 4;

		ostr.put('e');
		ostr.put('m');
		ostr.put('s');
		ostr.put('g');
		bytes_written += 4;
		ostr.put((uint8_t)version_);
		ostr.put(0u);
		ostr.put(0u);
		ostr.put(0u);
		bytes_written += 4;

		if (version_ == 1)
		{
			fmp4_write_uint32(timescale_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
			fmp4_write_uint64(presentation_time_, long_buf);
			ostr.write(long_buf, 8);
			bytes_written += 8;
			fmp4_write_uint32(event_duration_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
			fmp4_write_uint32(id_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
			ostr.write(scheme_id_uri_.c_str(), scheme_id_uri_.size() + 1);
			bytes_written += (uint32_t)scheme_id_uri_.size() + 1;
			ostr.write(value_.c_str(), value_.size() + 1);
			bytes_written += (uint32_t)value_.size() + 1;
		}
		else
		{
			ostr.write(scheme_id_uri_.c_str(), scheme_id_uri_.size() + 1);
			bytes_written += (uint32_t)scheme_id_uri_.size() + 1;
			ostr.write(value_.c_str(), value_.size() + 1);
			bytes_written += (uint32_t)value_.size() + 1;
			fmp4_write_uint32(timescale_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
			fmp4_write_uint32(presentation_time_delta_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
			fmp4_write_uint32(event_duration_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
			fmp4_write_uint32(id_, int_buf);
			ostr.write(int_buf, 4);
			bytes_written += 4;
		}
		if (message_data_.size())
			ostr.write((char *)&message_data_[0], message_data_.size());
		
		bytes_written += (uint32_t)message_data_.size();
		return bytes_written;
	}

	static bool f_compare_4cc(char* in, std::string in_4cc)
	{
		if (in[0] == in_4cc[0] && in[1] == in_4cc[1] && in[2] == in_4cc[2] && in[3] == in_4cc[3])
			return true;
		else
			return false;
	};

	// updated this function a bit to be a bit more generic
	uint32_t init_fragment::get_time_scale()
	{
		char *ptr = (char*)moov_box_.box_data_.data();

		bool trak_found = false;
		bool mdia_found = false;
		bool mdhd_found = false;
		uint32_t timescale = 1;

		uint32_t pos = 8;

		// find trak box 
		while (!trak_found && pos < moov_box_.box_data_.size())
		{
			if (f_compare_4cc(ptr + 4 + pos, "trak"))
				trak_found = true;
			else
				pos += fmp4_read_uint32(ptr + pos);
		}
		if (trak_found)
		{
			pos += 8;
			while(!mdia_found && pos < moov_box_.box_data_.size())
			{
				if (f_compare_4cc(ptr + 4 + pos, "mdia"))
					mdia_found = true;
				else
					pos += fmp4_read_uint32(ptr + pos);
			};
		}
		if (mdia_found)
		{
			pos += 8;
			while (!mdhd_found && pos < moov_box_.box_data_.size())
			{
				if (f_compare_4cc(ptr + 4 + pos, "mdhd"))
					mdhd_found = true;
				else
					pos += fmp4_read_uint32(ptr + pos);
			};
		}
		if (mdhd_found)
		{
			uint8_t version = (uint8_t)ptr[pos + 8];
			if (version == 1)
				timescale = fmp4_read_uint32(ptr + pos + 28);
			else
				timescale = fmp4_read_uint32(ptr + pos + 20);
			
			return timescale;
		}
		else
		    std::cout << "warning no mdhd box found, required for event message track format" << std::endl;
		    return timescale;
	
	}

	void media_fragment::parse_moof()
	{
		if (!moof_box_.size_)
			return;

		uint64_t box_size = 0;
		uint64_t offset = 8;
		uint8_t * ptr = &moof_box_.box_data_[0];

		while (moof_box_.box_data_.size() > offset)
		{
			unsigned int temp_off = 0;
			box_size = (uint64_t)fmp4_read_uint32((char *)&moof_box_.box_data_[offset]);

			char name[5] = { (char)ptr[offset + 4],(char)ptr[offset + 5],(char)ptr[offset + 6],(char)ptr[offset + 7],'\0' };

			if (box_size == 1) // the box_size is a large size (should not happen in fmp4)
			{
				temp_off = 8;
				box_size = fmp4_read_uint64((char *)& ptr[offset + temp_off]);
			}

			if (f_compare_4cc(name,"mfhd"))
			{
				mfhd_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				//cout << "mfhd size" << box_size << endl;
				continue;
			}

			if (f_compare_4cc(name,"trun"))
			{
				trun_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				continue;
			}

			if (f_compare_4cc(name,"tfdt"))
			{
				tfdt_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				continue;
			}

			if (f_compare_4cc(name,"tfhd"))
			{
				tfhd_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				continue;
			}

			// cmaf style only one traf box per moof so we skip it
			if (f_compare_4cc(name, "traf"))
			{
				offset += 8;
			}
			else
				offset += (unsigned int)box_size;
		}
	};

	void media_fragment::patch_tfdt(uint64_t patch, uint32_t seq_nr)
	{
		if (!moof_box_.size_)
			return;

		uint64_t box_size = 0;
		uint64_t offset = 8;

		if (seq_nr)
			this->mfhd_.seq_nr_ = seq_nr;

		this->tfdt_.base_media_decode_time_ = \
			this->tfdt_.base_media_decode_time_ + patch;

		// find the tfdt box and overwrite it
		while (moof_box_.box_data_.size() > offset)
		{
			unsigned int temp_off = 0;
			box_size = (uint64_t)fmp4_read_uint32((char *)&moof_box_.box_data_[offset]);
			uint8_t * ptr = &moof_box_.box_data_[0];
			char name[5] = { (char)ptr[offset + 4],(char)ptr[offset + 5],(char)ptr[offset + 6],(char)ptr[offset + 7],'\0' };

			if (box_size == 1) // the box_size is a large size (should not happen in fmp4)
			{
				temp_off = 8;
				box_size = fmp4_read_uint64((char *)& ptr[offset + temp_off]);
			}

			if (f_compare_4cc(name, "mfhd"))
			{
				
				if (seq_nr) 
					fmp4_write_uint32(seq_nr, (char *)&ptr[offset + temp_off + 12]);
				//mfhd_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				//cout << "mfhd size" << box_size << endl;
				continue;
			}

			if (f_compare_4cc(name, "trun"))
			{
				//trun_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				continue;
			}

			if (f_compare_4cc(name, "tfdt"))
			{
				
				this->tfdt_.version_ ? \
					fmp4_write_uint64(this->tfdt_.base_media_decode_time_, (char *)&ptr[offset + temp_off + 12]) : \
					fmp4_write_uint32((uint32_t)this->tfdt_.base_media_decode_time_, (char *)&ptr[offset + temp_off + 12]);

				offset += (uint64_t)box_size;
				continue;
			}

			if (f_compare_4cc(name, "tfhd"))
			{
				//tfhd_.parse((char *)& ptr[offset + temp_off]);
				offset += (uint64_t)box_size;
				continue;
			}

			// cmaf style only one traf box per moof so we skip it
			if (f_compare_4cc(name, "traf"))
			{
				offset += 8;
			}
			else
				offset += (unsigned int)box_size;
		}
	};

	// function to support the ingest, gets the init fragment data
	uint64_t ingest_stream::get_init_segment_data(std::vector<uint8_t> &init_seg_dat)
	{
		uint64_t ssize = init_fragment_.ftyp_box_.large_size_ + init_fragment_.moov_box_.large_size_;
		init_seg_dat.resize(ssize);

		// hard copy the init segment data to the output vector, as ftyp and moviefragment box need to be combined
		std::copy(init_fragment_.ftyp_box_.box_data_.begin(), 
			      init_fragment_.ftyp_box_.box_data_.end(), 
			      init_seg_dat.begin()
	    );
		std::copy(init_fragment_.moov_box_.box_data_.begin(), 
			      init_fragment_.moov_box_.box_data_.end(), 
			      init_seg_dat.begin() + init_fragment_.ftyp_box_.large_size_
		);

		return ssize;
	};

	// function to support the ingest of segments 
	uint64_t ingest_stream::get_media_segment_data(
		std::size_t index, std::vector<uint8_t> &media_seg_dat)
	{
		if (!(media_fragment_.size() > index))
			return 0;

		uint64_t ssize = media_fragment_[index].moof_box_.large_size_ + media_fragment_[index].mdat_box_.large_size_;
		media_seg_dat.resize(ssize);

		// copy moviefragmentbox
		std::copy(media_fragment_[index].moof_box_.box_data_.begin(), 
			      media_fragment_[index].moof_box_.box_data_.end(), 
			      media_seg_dat.begin()
	    );
		// copy mdat
		std::copy(media_fragment_[index].mdat_box_.box_data_.begin(), 
			      media_fragment_[index].mdat_box_.box_data_.end(), 
			      media_seg_dat.begin() + media_fragment_[index].moof_box_.large_size_
	    );

		return ssize;
	};

	//
	void media_fragment::print() const
	{
		for(int i=0;i<emsg_.size();i++)
		    if (emsg_[i].scheme_id_uri_.size() && !this->e_msg_is_in_mdat_)
			   emsg_[i].print();

		moof_box_.print(); // moof box size
		mfhd_.print(); // moof box header
		tfhd_.print(); // track fragment header
		tfdt_.print(); // track fragment decode time
		trun_.print(); // trun box
		mdat_box_.print(); // mdat 

		for (int i = 0; i<emsg_.size(); i++)
		    if (emsg_[i].scheme_id_uri_.size() && this->e_msg_is_in_mdat_)
			    emsg_[i].print();
	}

	// parse an fmp4 file for media ingest
	int ingest_stream::load_from_file(std::istream &infile, bool init_only)
	{
		try
		{
			if (infile)
			{
				std::vector<box> ingest_boxes;

				while (infile.good()) // read box by box in a vector
				{
					box b = {};
					if (b.read(infile))
						ingest_boxes.push_back(b);
					else  // break when we have boxes of size zero
						break;

					if (b.box_type_.compare("mfra") == 0)
						break;
				}

				// organize the boxes in init fragments and media fragments 
				for (auto it = ingest_boxes.begin(); it != ingest_boxes.end(); ++it)
				{
					if (f_compare_4cc( (char *)it->box_type_.c_str(), "ftyp"))
						init_fragment_.ftyp_box_ = *it;
					if (f_compare_4cc((char*)it->box_type_.c_str(), "moov"))
					{
						init_fragment_.moov_box_ = *it;
						if (init_only)
							return 0;
					}

					if (f_compare_4cc((char*)it->box_type_.c_str(), "moof")) // in case of moof box we push both the moof and following mdat
					{
						media_fragment m = {};
						m.moof_box_ = *it;
						bool mdat_found = false;
						auto prev_box = (it - 1);
						// see if there is an emsg before
						if (prev_box->box_type_.compare("emsg") == 0)
						{
							emsg e;
							e.parse((char *)& prev_box->box_data_[0], (unsigned int)prev_box->box_data_.size());
							m.emsg_.push_back(e);
							//cout << "|emsg|";
							//std::cout << "found inband dash emsg box" << std::endl;
						}
						//cout << "|moof|";
						while (!mdat_found)
						{
							it++;

							if (f_compare_4cc((char*)it->box_type_.c_str(), "mdat"))
							{
								// find event messages embedded in 

								m.mdat_box_ = *it;
								mdat_found = true;
								m.parse_moof();

								uint32_t index = 8;

								while (index < m.mdat_box_.box_data_.size())
								{
									// seek to next box in 
									uint8_t name[9] = {};
									for (uint32_t i = 0; i < 8; i++)
										name[i] = m.mdat_box_.box_data_[index + i];
									name[8] = '\0';
									uint32_t l_size = fmp4_read_uint32((char *)name);

									if (l_size > m.mdat_box_.box_data_.size())
										break;

									std::string enc_box_name((char *)&name[4]);

									if (f_compare_4cc((char*)enc_box_name.c_str(), "emsg") ) // right now we can only parse a single emsg, todo update to parse multiple emsg
									{
										emsg e = {};
										e.parse((char *)&m.mdat_box_.box_data_[index], (unsigned int)l_size);
										m.emsg_.push_back(e);
										m.e_msg_is_in_mdat_ = true;
										index += l_size;
										continue;
									}
									if (f_compare_4cc((char*)enc_box_name.c_str(), "embe"))
									{
										index += l_size;
										continue;
									}
									break; // skip on all other mdat content only embe and emsg allowed
								}

								media_fragment_.push_back(m);

								break;
							}
						}
					}
					if (it->box_type_.compare("mfra") == 0)
					{
						this->mfra_box_ = *it;
					}
					if (it->box_type_.compare("sidx") == 0)
					{
						this->sidx_box_ = *it;
						std::cout << "|sidx|";
					}
					if (it->box_type_.compare("meta") == 0)
					{
						this->meta_box_ = *it;
						std::cout << "|meta|";
					}
				}
			}
			std::cout << std::endl;
			std::cout << "***  finished reading fmp4 fragments  ***" << std::endl;
			std::cout << "***  read  fmp4 init fragment         ***" << std::endl;
			std::cout << "***  read " << media_fragment_.size() << " fmp4 media fragments ***" << std::endl;

			return 0;
		}
		catch (std::exception e)
		{
			std::cout << e.what() << std::endl;
			return 0;
		}
	}

	// archival function, write init segment to a file
	int ingest_stream::write_init_to_file(
		std::string &ofile, 
		unsigned int nfrags, 
		bool write_sep_files)
	{
		// write the stream to an output file
		if (!write_sep_files) {
			std::ofstream out_file(ofile, std::ofstream::binary);

			if (out_file.good())
			{
				std::vector<uint8_t> init_data;
				get_init_segment_data(init_data);
				out_file.write((char *)init_data.data(), init_data.size());
				for (unsigned int k = 0; k < nfrags; k++) {
					if (k < media_fragment_.size()) {
						init_data.clear();
						get_media_segment_data(k, init_data);
						out_file.write((char *)init_data.data(), init_data.size());
					}
				}
				out_file.close();
				std::cout << " done written init segment to file: " << ofile << std::endl;
			}
			else
			{
				std::cout << " error writing stream to file " << std::endl;
			}
		}
		else 
		{
			std::vector<uint8_t> init_data;
			get_init_segment_data(init_data);
			const unsigned int chunk_count=4;
			for (unsigned int k = 0; k < nfrags; k+=chunk_count) 
			{
				std::ofstream out_file( std::to_string(k/chunk_count) + "_" + ofile, std::ofstream::binary);
				std::vector<uint8_t> segment_data;
				if (out_file.good())
				{
					out_file.write((char *)init_data.data(), init_data.size());
					for (unsigned int i = 0; i < chunk_count; i++) {
						segment_data.clear();
						get_media_segment_data(k*chunk_count + i, segment_data);
						out_file.write((char *)segment_data.data(), segment_data.size());
					}
					out_file.close();
				}
			}
		}
		return 0;
	}

	// warning only use with the testes=d pre-encoded moov boxes to write streams
	bool set_track_id(std::vector<uint8_t> &moov_in, uint32_t track_id)
	{
		bool set_tkhd = false;

		for (std::size_t k = 0; k < moov_in.size() - 16; k++)
		{
			if (std::string((char *)&moov_in[k]).compare("tkhd") == 0)
			{
				//cout << "tkhd found" << endl;
				if ((uint8_t)moov_in[k + 4] == 1)
				{
					// version 1 
					fmp4_write_uint32(track_id, (char*)&moov_in[k + 24]);
					set_tkhd = true;
				}
				else {
					// version 0
					fmp4_write_uint32(track_id, (char*)&moov_in[k + 16]);
					set_tkhd = true;
				}
			}
			if (std::string((char *)&moov_in[k]).compare("trex") == 0)
			{
				//cout << "tkhd found" << endl;
				if ((uint8_t)moov_in[k + 4] == 1)
				{
					// version 1 
					fmp4_write_uint32(track_id, (char*)&moov_in[k + 24]);
					set_tkhd = true;
				}
				else {
					// version 0
					fmp4_write_uint32(track_id, (char*)&moov_in[k + 16]);
					set_tkhd = true;
				}
			}
		}
		return true;
	}

	void ingest_stream::print() const
	{
		for (auto it = media_fragment_.begin(); it != media_fragment_.end(); it++)
		{
			it->print();
		}
	}

	// method to patch all timestamps from a VoD (0) base to live (epoch based)
	void ingest_stream::patch_tfdt(uint64_t time_anchor, bool applytimescale, uint32_t anchor_scale)
	{
		if (applytimescale) {
			uint32_t timescale = 0;
			timescale = this->init_fragment_.get_time_scale();
			if(timescale)
			   time_anchor = time_anchor * timescale / anchor_scale; // offset to add to the timestamps
		}

		uint32_t seq_nr = 0;
		
		// only patch sequence number in the case when no timescale is applied 
		if (media_fragment_.size() && !applytimescale)
			seq_nr = 1 + this->media_fragment_[media_fragment_.size()-1].mfhd_.seq_nr_;

		for (uint32_t i = 0; i < this->media_fragment_.size(); i++)
		{
				this->media_fragment_[i].patch_tfdt(time_anchor, seq_nr + i);
		}	
	}

	uint64_t ingest_stream::get_duration() 
	{
		if (media_fragment_.size() >= 2) 
		{
			// todo add aditional checks for timing behavior
			return media_fragment_[media_fragment_.size() - 1].tfdt_.base_media_decode_time_
				- media_fragment_[0].tfdt_.base_media_decode_time_
				+ media_fragment_[media_fragment_.size() - 1].get_duration();
		}
		if (media_fragment_.size() == 1)
		{
			// todo add aditional checks for timing behavior
			return media_fragment_[media_fragment_.size() - 1].get_duration();
		}
		return 0;
	}
	
	uint64_t ingest_stream::get_start_time() 
	{
		if (media_fragment_.size() >= 1)
		{
			// todo add aditional checks for timing behavior
			return media_fragment_[0].tfdt_.base_media_decode_time_;
		}
		return 0;
	}
}
