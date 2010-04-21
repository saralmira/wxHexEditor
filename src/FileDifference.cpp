/***********************************(GPL)********************************
*   wxHexEditor is a hex edit tool for editing massive files in Linux   *
*   Copyright (C) 2010  Erdem U. Altinyurt                              *
*                                                                       *
*   This program is free software; you can redistribute it and/or       *
*   modify it under the terms of the GNU General Public License         *
*   as published by the Free Software Foundation; either version 2      *
*   of the License.                                                     *
*                                                                       *
*   This program is distributed in the hope that it will be useful,     *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of      *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       *
*   GNU General Public License for more details.                        *
*                                                                       *
*   You should have received a copy of the GNU General Public License   *
*   along with this program;                                            *
*   if not, write to the Free Software Foundation, Inc.,                *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA        *
*                                                                       *
*               home  : wxhexeditor.sourceforge.net                     *
*               email : death_knight at gamebox.net                     *
*************************************************************************/
#include "FileDifference.h"
#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(ArrayOfNode);

FileDifference::FileDifference(wxFileName& myfilename, FileAccessMode FAM ){
	file_access_mode = FAM;
	the_file = myfilename;
	if(myfilename.IsFileReadable()){//FileExists()){
//		if( myfilename.GetFullPath() == wxT("/dev/mem") ){
//			//Ram device in Unix has need special treatment.
//			}
//		else
			{
			if( FAM == ReadOnly)
				Open(myfilename.GetFullPath(), wxFile::read);
			else
				Open(myfilename.GetFullPath(), wxFile::read_write);

			if(! IsOpened()){
				file_access_mode = AccessInvalid;
				wxMessageDialog *dlg = new wxMessageDialog(NULL,_("File cannot open."),_("Error"), wxOK|wxICON_ERROR, wxDefaultPosition);
				dlg->ShowModal();dlg->Destroy();
				}
			}
		}
	}

FileDifference::~FileDifference(){
	DiffArray.Clear();
	}

bool FileDifference::SetAccessMode( FileAccessMode fam ){
	if( Access( the_file.GetFullPath() , (fam == ReadOnly ? wxFile::read : wxFile::read_write) ) ){
		Close();
		Open( the_file.GetFullPath(), (fam == ReadOnly ? wxFile::read : wxFile::read_write) );
		if(! IsOpened()){
			wxBell();wxBell();wxBell();
			wxMessageDialog *dlg = new wxMessageDialog(NULL,_("File load error!.\nFile closed but not opened while access change. For avoid corruption close the program"),_("Error"), wxOK|wxICON_ERROR, wxDefaultPosition);
			dlg->ShowModal();dlg->Destroy();
			file_access_mode = AccessInvalid;
			return false;
			}
		file_access_mode = fam;
		return true;
		}
	wxBell();
	wxMessageDialog *dlg = new wxMessageDialog(NULL,wxString(_("File cannot open in ")).Append( FAMtoString( fam) ).Append(_(" mode.")),_("Error"), wxOK|wxICON_ERROR, wxDefaultPosition);
	dlg->ShowModal();dlg->Destroy();
	return false;
	}

int FileDifference::GetAccessMode( void ){
	return file_access_mode;
	}

wxString FileDifference::GetAccessModeString( void ){
	return FAMtoString( file_access_mode );
	}

wxString FileDifference::FAMtoString( FileAccessMode& FAM ){
	return FAM == ReadOnly ? _T("Read-Only") :
		FAM == ReadWrite ? _T("Read-Write") :
		FAM == DirectWrite ? _T("Direct-Write") : _("Access Invalid");
	}

wxFileName FileDifference::GetFileName( void ){
	return the_file;
	}

DiffNode* FileDifference::NewNode( uint64_t start_byte, const char* data, int64_t size, bool inject ){
	DiffNode* newnode = new struct DiffNode( start_byte, size, inject );
	if( size < 0 ){//Deletion!
		newnode->old_data = new char[-size];
		if( newnode->old_data == NULL )
			wxLogError( _("Not Enought RAM") );
		else{
			Seek( start_byte, wxFromStart );
			Read( newnode->old_data, -size );
			return newnode;
			}
		}
	else if( inject ){
		newnode->new_data = new char[size];
		if( newnode->new_data == NULL )
			wxLogError( _("Not Enought RAM") );
		else{
			memcpy( newnode->new_data, data, size);
			return newnode;
			}
		}
	else{// Normal opeariton
		newnode->old_data = new char[size];
		newnode->new_data = new char[size];
		if( newnode->old_data == NULL || newnode->new_data == NULL){
			wxLogError( _("Not Enought RAM") );
			delete newnode->old_data;
			delete newnode->new_data;
			return NULL;
			}
		else{
			memcpy( newnode->new_data, data, size);
			Seek( start_byte, wxFromStart );
			Read( newnode->old_data, size );
			return newnode;
			}
		}
	return NULL;
	}

bool FileDifference::IsChanged( void ){
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ )
		if( DiffArray[i]->flag_commit == DiffArray[i]->flag_undo )	// means this node has to be (re)writen to disk
			return true;
	return false;
	}

bool FileDifference::Apply( void ){
	bool success=true;
	if( file_access_mode != ReadOnly )
		for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ ){
			if( DiffArray[i]->flag_commit == DiffArray[i]->flag_undo ){		// If there is unwriten data
				Seek(DiffArray[i]->start_offset, wxFromStart);			// write it
				success*=Write(DiffArray[i]->new_data, DiffArray[i]->size);
				DiffArray[i]->flag_commit = DiffArray[i]->flag_commit ? false : true;	//alter state of commit flag
				}
		}
	return success;
	}

int64_t FileDifference::Undo( void ){
	if( DiffArray.GetCount() == 0 ){
		wxBell();						// No possible undo action here bell
		return -1;
		}
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ )
		if( DiffArray.Item(i)->flag_undo ){		//find first undo node if available
			if( i != 0 ){			//if it is not first node
				DiffArray.Item( i-1 )->flag_undo = true;	//set previous node as undo node
				if( file_access_mode == DirectWrite )		//Direct Write mode is always applies directly.
					Apply();
				return DiffArray.Item( i-1 )->start_offset;//and return undo transaction start for screen focusing
				}
			else{						//if first node is undo
				wxBell();				//ring the bell
				return -1;				//this means there is not undo node available
				}
			}
	DiffArray.Last()->flag_undo=true;
	return DiffArray.Last()->start_offset;
	}
bool FileDifference::IsAvailable_Undo( void ){
	if( DiffArray.GetCount() == 0 )
		return false;
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ )
		if( !DiffArray.Item(i)->flag_undo )
			return true;
	return false;
	}

int64_t FileDifference::Redo( void ){
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ )
		if( DiffArray.Item(i)->flag_undo ){		//find first undo node if available
			DiffArray.Item(i)->flag_undo = false;
			if( file_access_mode == DirectWrite )	//Direct Write mode is always applies directly.
				Apply();
			return DiffArray.Item(i)->start_offset;
			}
	wxBell();	// No possible redo action
	return -1;
	}
bool FileDifference::IsAvailable_Redo( void ){
	if( DiffArray.GetCount() == 0 )
		return false;
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ )
		if( DiffArray.Item(i)->flag_undo )
			return true;
	return false;
	}

void FileDifference::ShowDebugState( void ){
	std::cout << "\n\nNumber\t" << "Start\t" << "Size\t" << "DataN\t" << "DataO\t" << "writen\t" << "Undo\n";
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ ){
		std::cout << i << '\t' << DiffArray[i]->start_offset << '\t' << DiffArray[i]->size << '\t';
		if (DiffArray[i]->size > 0 )
			std::cout << std::hex <<  (unsigned short)DiffArray[i]->new_data[0] << '\t'
				<< (unsigned short)DiffArray[i]->old_data[0] << '\t' <<	DiffArray[i]->flag_commit << '\t' << DiffArray[i]->flag_undo  << std::dec << std::endl;
		else
			std::cout << std::hex <<  0 << '\t' << (unsigned short)DiffArray[i]->old_data[0] << '\t' <<	DiffArray[i]->flag_commit << '\t' << DiffArray[i]->flag_undo  << std::dec << std::endl;
		}
	}

wxFileOffset FileDifference::Length( void ){
	#ifdef __WXGTK__
		if( the_file.GetFullPath() == wxT("/dev/mem") ){
			return 512*1024*1024;
			}
	#endif
	if(! IsOpened() )
		return -1;
	wxFileOffset max_size=wxFile::Length();
//	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ )
//		max_size = (( DiffArray[i]->start_offset + DiffArray[i]->size ) > max_size ) ? ( DiffArray[i]->start_offset + DiffArray[i]->size ):( max_size );

	return max_size + GetByteMovements( max_size );
	}

int64_t FileDifference::GetByteMovements( uint64_t current_location ){
		/** MANUAL of Code understanding**
	current_location 												= [
	current_location + size 									= ]
	DiffArray[i]'s delete start_offset						= (
	DiffArray[i]'s delete start_offset + it's size	= )
	DiffArray[i]'s inject start_offset						= {
	DiffArray[i]'s inject start_offset + it's size	= }
	data																	= ......
	important data													= ,,,
	**/
	int64_t cnt = 0;
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ ){
		if( DiffArray[i]->flag_inject and DiffArray[i]->flag_undo and !DiffArray[i]->flag_commit )	// Allready committed to disk, nothing to do here
			continue;

		///For Deletion First:
		if( DiffArray[i]->size < 0 ){
			DiffNode *Delete_Node = DiffArray[i];	//For easy reading of code
			///State ...(...)...[...],,,. -> ...()......[,,,].
			///State ...(..[..).].,,,.... -> ...()..[,,,]....
			///State ...(..[...].)..,,,.. -> ...()..[,,,]..
			if( Delete_Node->start_offset <= current_location ){
				cnt += Delete_Node->size;	//returns minus movements
				}
			///State ...[...(...)...]... and  ...[...(...]...) will not handled here.
			}
		///Than injections:
		else if( DiffArray[i]->flag_inject ){
			DiffNode *Injection_Node = DiffArray[i];
			///State ...{},,,[...]... -> ...{...}[...]......
			if( Injection_Node->start_offset + Injection_Node->size <= current_location ){
				cnt += Injection_Node->size;//returns plus movements
				}
			///State ...{},,,[...]... -> ...{....[.},,],....
			///State ...{},,,[...]... -> ...{....[...]..},,,
			///State ...[.{},],,..... -> ...[.{..].},,,.....
			///State ...[.{}..]...... -> ...[.{..}.]........ will not handled here.
			}
		}
	return cnt;
	}

bool FileDifference::ReadByte( char *chr, uint64_t location ){
	int64_t BM = GetByteMovements( location );
	wxFile::Seek( location-BM );
	return wxFile::Read( chr, 1);	//Returns 1 if not error or 0 on error
	}

long FileDifference::Read( char* buffer, int size){
	int64_t current_location = Tell();
	int64_t move_start = GetByteMovements( current_location );
	int64_t move_end = GetByteMovements( current_location+size );
	int64_t real_read_location = current_location - move_start;
	int64_t real_read_size = size + (move_start - move_end);
	real_read_location = real_read_location < 0 ? 0 : real_read_location;
#ifdef _DEBUG_FILE_
	std::cout << "Current loc:" << std::dec << current_location
				<< " Size  " << size
				<< " Move Start:" << move_start
				<< " Move End " << move_end
				<< " Real Read Loc:" << real_read_location
				<< " Real Read Size: " << real_read_size
				<< " File Length: " << Length() << std::endl;
#endif

#if 0
	wxFile::Seek( real_read_location );
	int readsize = wxFile::Read(buffer,size);	//Reads file as wxFile::Lenght
#else
	//This code is proper and working but not works with coriginal diff engine. Needed to make another one which understands deletions.
	int readsize=0;
	char chr;
	///Since we hanled other states and:
	///State ...[...(...)...]... and  ...[...(...]...)
	///Are invulranable for 1 byte reading, there is no problem about this approach.
	///But this will slow down reading projecs significantly.
	///Anyway it's already superfeast process...
	for( int i = 0 ; i < size ; i++ ){
		if( ReadByte( &chr, current_location+i) ){
			buffer[i]=chr;
			readsize++;
			}
		else
			break;
		}
#endif
//	if(real_read_location != readsize)				//If there is free chars
//		readsize = (Length() - current_location > size) ? (size):(Length() - current_location);	//check for buffer overflow
	char *data=buffer;

	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ ){
		/// MANUAL of Code understanding
		///current_location 						= [
		///current_location + size 				= ]
		///DiffArray[i]'s start_offset				= (
		///DiffArray[i]'s start_offset + it's size	= )
		///data									= ...
		///changed data							= xxx

		if( not DiffArray[i]->flag_inject ){
			if( DiffArray[i]->flag_undo && !DiffArray[i]->flag_commit )	// Allready committed to disk, nothing to do here
				continue;

			//Makes correction on reading file.
			int64_t movement=0;

			for( int j=i ; j< DiffArray.GetCount() ; j++){
				if( DiffArray[j]->flag_inject
						 and DiffArray[j]->start_offset - movement <=  DiffArray[i]->start_offset //Delete node j + movement ıs smaller than start offset?
						 and not DiffArray[j]->flag_undo ){
					std::cout << "Move for patch i:" << i << " DiffArray[i]->start_offset::" << DiffArray[i]->start_offset << " DiffArray[j]->start_offset + movement:" << DiffArray[j]->start_offset + movement << std::endl;
					movement += DiffArray[j]->size;
					}
				}
			std::cout << "Movement detected for patch i:" << i << " movement size:" << movement << std::endl;

			///State: ...[...(xxx]xxx)...
			if(current_location <= DiffArray[i]->start_offset+movement && current_location+size >= DiffArray[i]->start_offset+movement){
				int irq_loc = DiffArray[i]->start_offset +movement - current_location;
				//...[...(xxx)...]... //not neccessery, this line includes this state
				int irq_size = (size - irq_loc > DiffArray[i]->size) ? (DiffArray[i]->size) : (size - irq_loc);
				memcpy(data+irq_loc , DiffArray[i]->flag_undo ? DiffArray[i]->old_data : DiffArray[i]->new_data, irq_size );
				}

			///State: ...(xxx[xxx)...]...
			else if (current_location <= DiffArray[i]->start_offset +movement + DiffArray[i]->size && current_location+size >= DiffArray[i]->start_offset -movement + DiffArray[i]->size){
				int irq_skipper = current_location - DiffArray[i]->start_offset +movement;	//skip this bytes from start
				int irq_size = DiffArray[i]->size - irq_skipper;
				memcpy(data, DiffArray[i]->flag_undo ? DiffArray[i]->old_data : DiffArray[i]->new_data + irq_skipper, irq_size );
				}

			///State: ...(xxx[xxx]xxx)...
			else if(DiffArray[i]->start_offset +movement <= current_location && DiffArray[i]->start_offset+movement + DiffArray[i]->size >= current_location+size){
				int irq_skipper = current_location - DiffArray[i]->start_offset+movement;	//skip this bytes from start
				memcpy(data, DiffArray[i]->flag_undo ? DiffArray[i]->old_data : DiffArray[i]->new_data + irq_skipper, size );
				}
			}
		}

	return readsize;
	}

bool FileDifference::Add( uint64_t start_byte, const char* data, int64_t size, bool injection ){
	//Check for undos first
	for( unsigned i=0 ; i < DiffArray.GetCount() ; i++ ){
		if( DiffArray[i]->flag_undo ){
			if(DiffArray.Item(i)->flag_commit ){	// commited undo node
				DiffArray[i]->flag_undo = false;		//we have to survive this node as it unwriten, non undo node
				DiffArray[i]->flag_commit = false;
				char *temp_buffer = DiffArray[i]->old_data;	//swap old <-> new data
				DiffArray[i]->old_data = DiffArray[i]->new_data;
				DiffArray[i]->new_data = temp_buffer;
				}
			else{									// non committed undo node
				while( i < (DiffArray.GetCount()) ){	// delete beyond here
					#ifdef _DEBUG_FILE_
						std::cout << "DiffArray.GetCount() : " << DiffArray.GetCount() << " while i = " << i<< std::endl;
					#endif
					DiffNode *temp;
					temp = *(DiffArray.Detach( i ));
					delete temp;
					}
				break;								// break for addition
				}
			}
		}
	//Adding node here
	DiffNode *rtn = NewNode( start_byte, data, size, injection );
		if( rtn != NULL ){
			DiffArray.Add( rtn );						//Add new node to tail
			if( file_access_mode == DirectWrite )	//Direct Write mode is always applies directly.
				Apply();
			return true;
			}
		else
			return false;							//if not created successfuly
	}
